
#include "parser.h"

#include <cassert>
#include <cstdio>
#include <charconv>

bool binary_writer::reset(const char* path) noexcept
{
    if (file)
    {
        flush_buffer();
        std::fclose(file);
        file = nullptr;
    }

    if (auto err = fopen_s(&file, path, "wb"))
    {
        std::println("{}: error: Failed to open file for writing", path);
        std::println("{}: error: {} ({}),", path, std::generic_category().message(err), err);
        return false;
    }

    return true;
}

bool binary_writer::write_bytes(const std::uint8_t* data, std::size_t size) noexcept
{
    // Write any pending data in 'buffer'
    flush_buffer();
    return std::fwrite(data, 1, size, file) == size;
}

bool binary_writer::write_bits(const std::uint32_t* data, std::size_t size, std::uint8_t bitsInLast) noexcept
{
    while (size > 0)
    {
        std::uint8_t bits = (size == 1) ? bitsInLast : 32;
        std::uint32_t value = *data;

        while (bits > 0)
        {
            if (bit_index == 0)
            {
                buffer[write_index] = 0;
            }

            auto writeSize = std::min(bits, static_cast<std::uint8_t>(8 - bit_index));
            auto mask = static_cast<std::uint8_t>(0xFF >> (8 - writeSize));

            buffer[write_index] |= (value & mask) << bit_index;
            value >>= writeSize;
            bits -= writeSize;

            bit_index = (bit_index + writeSize) % 8;
            if (bit_index == 0) // We've filled the byte
            {
                // We'll overflow back to zero when we've exhausted the buffer
                if (++write_index == 0)
                {
                    if (std::fwrite(buffer, 1, 256, file) != 256)
                    {
                        return false;
                    }
                    // At this point, both indices have been reset
                }
            }
        }

        ++data;
        --size;
    }

    return true;
}

template <typename T>
bool parse_numeric_value(std::string_view text, T& value, int base = 10)
{
    auto begin = text.data();
    auto end = begin + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, value, base);
    return (ec == std::errc()) && (ptr == end);
}

bool parser::parse(const char* path)
{
    // Reset in case this object is being reused
    root = nullptr;
    state = {};

    if (!lex.open(path))
    {
        return false;
    }

    root = std::make_unique<scope>();
    while (lex)
    {
        if (!parse_output(root.get()))
        {
            return false;
        }
    }

    return !lex.saw_error();
}

bool parser::parse_output(scope* parent, bool isScoped)
{
    std::unique_ptr<output_node> currOutput;
    bool keepGoing = true;
    while (keepGoing)
    {
        if (!currOutput)
        {
            // Output mode changed or was "interrupted" (e.g. by a repeat); create a new one
            switch (state.output_mode)
            {
            case 1: // Binary (bit) output
                currOutput = std::make_unique<binary_output>();
                break;

            case 8:  // Byte output
            case 16: // Word output
                currOutput = std::make_unique<byte_output>();
                break;

            default:
                assert(false); // Invalid
#if __cpp_lib_unreachable >= 202202L
                std::unreachable();
#endif
                break;
            }
        }

        auto tok = lex.next();
        switch (tok.kind)
        {
        case token_kind::init:
            assert(false); // Internal only value that should get overwritten
            break;

        case token_kind::invalid:
            // Skip past invalid tokens
            break;

        case token_kind::eof:
            keepGoing = false;
            break;

        case token_kind::value:
        {
            std::string valueStr(tok.text); // String may no longer be valid after calling 'peek'
            auto valueLoc = tok.location;
            tok = lex.peek();
            if (tok.kind == token_kind::elipsis)
            {
                lex.next(); // Consume elipsis
                tok = lex.peek();
                if (tok.kind != token_kind::value)
                {
                    lex.emit_error(tok.location, "Unexpected token; expected a numeric value after '...'");
                    break;
                }
                lex.next(); // Consume value

                // This is a range
                int delta = 1;
                auto setupRange = [&](auto& begin, auto& end) {
                    static_assert(std::is_same_v<decltype(begin), decltype(end)>);
                    const char* sizeStr = (state.output_mode == 1)   ? "an arbitrarily sized"
                                          : (state.output_mode == 8) ? "an 8-bit"
                                                                     : "a 16-bit";
                    if (!parse_numeric_value(valueStr, begin, state.input_mode))
                    {
                        lex.emit_error(valueLoc, "Unexpected token; expected {} base-{} numeric value", sizeStr, state.input_mode);
                        return false;
                    }
                    if (!parse_numeric_value(tok.text, end, state.input_mode))
                    {
                        lex.emit_error(tok.location, "Unexpected token; expected {} base-{} numeric value", sizeStr, state.input_mode);
                        return false;
                    }

                    if (end < begin)
                    {
                        delta = -1;
                    }

                    return true;
                };

                switch (state.output_mode)
                {
                case 1:
                {
                    assert(state.input_mode == 2); // Only valid input mode for binary output

                    // Special case: ranges in bit output mode must be identical lengths
                    if (valueStr.size() != tok.text.size())
                    {
                        lex.emit_error(tok.location, "Bit ranges must be the same length");
                        break;
                    }
                    else if (valueStr.size() > 64)
                    {
                        // NOTE: Normally binary inputs are unbounded in size. Since we're generating a sequence, it's
                        // reasonable to impose a 64-bit limit
                        lex.emit_error(valueLoc, "Bit ranges must be 64 bits or less");
                        break;
                    }

                    std::uint64_t begin, end;
                    if (!setupRange(begin, end))
                    {
                        break;
                    }

                    // NOTE: while(true) since this is an *inclusive* range and the end value may not be incrementable
                    auto bitCount = static_cast<std::uint8_t>(valueStr.size());
                    while (true)
                    {
                        // We still need to flip the bits. We can't do that outside the loop, otherwise incrementing
                        // would be somewhat difficult
                        std::uint64_t value = 0;
                        std::uint64_t unshiftedValue = begin;
                        for (std::size_t i = 0; i < bitCount; ++i)
                        {
                            value = (value << 1) | (unshiftedValue & 0x01);
                            unshiftedValue >>= 1;
                        }

                        static_cast<binary_output*>(currOutput.get())
                            ->add_bits(static_cast<std::uint32_t>(value), std::min(bitCount, static_cast<std::uint8_t>(32)));
                        if (bitCount > 32)
                        {
                            static_cast<binary_output*>(currOutput.get())->add_bits(static_cast<std::uint32_t>(value >> 32), bitCount - 32);
                        }

                        // Exits the loop when we just wrote end (== begin) to the output
                        if (begin == end)
                        {
                            break;
                        }
                        begin = static_cast<std::uint64_t>(begin + delta);
                    }

                    break;
                }

                case 8:
                {
                    std::uint8_t begin, end;
                    if (!setupRange(begin, end))
                    {
                        break;
                    }

                    while (true)
                    {
                        static_cast<byte_output*>(currOutput.get())->add_byte(begin);
                        if (begin == end) // Inclusive range
                        {
                            break;
                        }
                        begin = static_cast<std::uint8_t>(begin + delta);
                    }
                    break;
                }

                case 16:
                {
                    std::uint16_t begin, end;
                    if (!setupRange(begin, end))
                    {
                        break;
                    }

                    while (true)
                    {
                        static_cast<byte_output*>(currOutput.get())->add_word(begin);
                        if (begin == end) // Inclusive range
                        {
                            break;
                        }
                        begin = static_cast<std::uint16_t>(begin + delta);
                    }
                    break;
                }
                }
            }
            else
            {
                // This is a single value
                switch (state.output_mode)
                {
                case 1:
                {
                    assert(state.input_mode == 2); // Only valid input mode for binary output

                    std::size_t readIndex = state.reverse_output ? 0 : (valueStr.size() - 1);
                    std::ptrdiff_t delta = state.reverse_output ? 1 : -1;

                    // This value can be arbitrarily long, so process in 32-bit chunks
                    bool validValue = true;
                    for (std::size_t i = 0; i < valueStr.size();)
                    {
                        std::uint32_t value = 0;
                        std::uint8_t bits = 0;
                        while ((i < valueStr.size()) && (bits < 32))
                        {
                            if (valueStr[readIndex] != '0' && valueStr[readIndex] != '1')
                            {
                                validValue = false;
                                lex.emit_error(valueLoc, "Unexpected token; expected a binary value");
                                break;
                            }

                            value |= static_cast<std::uint32_t>((valueStr[readIndex] == '1') ? 1 : 0) << bits;
                            readIndex += delta;
                            ++i;
                            ++bits;
                        }

                        if (!validValue)
                        {
                            break;
                        }

                        static_cast<binary_output*>(currOutput.get())->add_bits(value, bits);
                    }
                    break;
                }

                case 8:
                {
                    std::uint8_t value = 0;
                    if (!parse_numeric_value(valueStr, value, state.input_mode))
                    {
                        lex.emit_error(valueLoc, "Unexpected token; expected an 8-bit base-{} numeric value", state.input_mode);
                        break;
                    }
                    static_cast<byte_output*>(currOutput.get())->add_byte(value);
                    break;
                }

                case 16:
                {
                    std::uint16_t value = 0;
                    if (!parse_numeric_value(valueStr, value, state.input_mode))
                    {
                        lex.emit_error(valueLoc, "Unexpected token; expected a 16-bit base-{} numeric value", state.input_mode);
                        break;
                    }
                    static_cast<byte_output*>(currOutput.get())->add_word(value);
                    break;
                }
                }
            }
            break;
        }

        case token_kind::kw_repeat:
        {
            // Repeat blocks save & restore the input/output mode
            auto savedState = state;
            if (!currOutput->is_empty())
            {
                parent->add_child(std::move(currOutput));
                currOutput = nullptr; // To be reset on next iteration of the loop
            }

            // Verify the input. Expect 'repeat (count) { ... }'
            tok = lex.next();
            if (tok.kind != token_kind::paren_open)
            {
                lex.emit_error(tok.location, "Unexpected token; expected '(' after 'repeat'");
                break;
            }

            tok = lex.next();
            if (tok.kind != token_kind::value)
            {
                lex.emit_error(tok.location, "Unexpected token; expected a numeric argument to 'repeat'");
                break;
            }

            std::uint32_t count = 0;
            if (!parse_numeric_value(tok.text, count))
            {
                lex.emit_error(tok.location, "Invalid numeric argument to 'repeat'");
                break;
            }

            tok = lex.next();
            if (tok.kind != token_kind::paren_close)
            {
                lex.emit_error(tok.location, "Unexpected token; expected ')' after numeric argument to 'repeat'");
                break;
            }

            tok = lex.next();
            if (tok.kind != token_kind::curly_open)
            {
                lex.emit_error(tok.location, "Unexpected token; expected '{{' to begin 'repeat' scope");
                break;
            }

            auto repeatNode = std::make_unique<repeat>(count);
            if (!parse_output(repeatNode.get(), true))
            {
                return false;
            }

            // NOTE: The call to 'parse_output' will consume the closing curly brace
            parent->add_child(std::move(repeatNode));

            state = savedState;
            break;
        }

        case token_kind::output_binary:
        case token_kind::output_binary_reversed:
            if (state.output_mode != 1)
            {
                parent->add_child(std::move(currOutput));
                currOutput = nullptr; // To be reset on next iteration of the loop
            }
            state.output_mode = 1;
            state.reverse_output = tok.kind == token_kind::output_binary_reversed;
            state.input_mode = 2;
            break;

        case token_kind::output_byte:
            if (state.output_mode != 8)
            {
                parent->add_child(std::move(currOutput));
                currOutput = nullptr; // To be reset on next iteration of the loop
            }
            state.output_mode = 8;
            state.reverse_output = false;
            state.input_mode = 16;
            break;

        case token_kind::output_word:
            if (state.output_mode != 16)
            {
                parent->add_child(std::move(currOutput));
                currOutput = nullptr; // To be reset on next iteration of the loop
            }
            state.output_mode = 16;
            state.reverse_output = false;
            state.input_mode = 16;
            break;

        case token_kind::input_binary:
            // Binary input always allowed
            state.input_mode = 2;
            break;

        case token_kind::input_decimmal:
            // Only allowed if output mode is not binary
            if (state.output_mode == 1)
            {
                lex.emit_error(tok.location, "Cannot change input mode to decimal when output mode is binary");
            }
            else
            {
                state.input_mode = 10;
            }
            break;

        case token_kind::input_hexadecimal:
            // Only allowed if output mode is not binary
            if (state.output_mode == 1)
            {
                lex.emit_error(tok.location, "Cannot change input mode to hexadecimal when output mode is binary");
            }
            else
            {
                state.input_mode = 16;
            }
            break;

        case token_kind::elipsis:
            // We should consume the elipsis after reading the first value, hence this is an error
            lex.emit_error(tok.location, "'...' unexpected at this time. Did you mean to create a range (begin...end)?");
            break;

        case token_kind::paren_open:
        case token_kind::paren_close:
        case token_kind::curly_open:
            // Only valid as a part of 'repeat' which we consume above
            lex.emit_error(tok.location, "'{}' unexpected at this time", tok.text);
            break;

        case token_kind::curly_close:
            // Only valid when part of a scope (i.e. repeat block)
            if (!isScoped)
            {
                lex.emit_error(tok.location, "'}}' unexpected at this time");
            }
            else
            {
                isScoped = false; // Avoid error below
                keepGoing = false;
            }
            break;

        case token_kind::string:
            // Only valid when in byte output mode
            if (state.output_mode != 8)
            {
                lex.emit_error(tok.location, "Strings are only allowed when output mode is byte");
            }
            else
            {
                // The token text contains the quotes; strip them now
                assert(tok.text.size() >= 2);
                auto text = tok.text.substr(1, tok.text.size() - 2);
                for (std::size_t i = 0; i < text.size(); ++i)
                {
                    if (text[i] != '\\')
                    {
                        static_cast<byte_output*>(currOutput.get())->add_byte(text[i]);
                    }
                    else
                    {
                        bool valid = true;
                        std::uint8_t byte = 0;
                        switch (text[++i])
                        {
                        case '0':
                            byte = '\0';
                            break;
                        case 'n':
                            byte = '\n';
                            break;
                        case 'r':
                            byte = '\r';
                            break;
                        case 't':
                            byte = '\t';
                            break;
                        case 'b':
                            byte = '\b';
                            break;
                        case 'f':
                            byte = '\f';
                            break;
                        case 'v':
                            byte = '\v';
                            break;
                        case '\\':
                            byte = '\\';
                            break;
                        case '"':
                            byte = '"';
                            break;
                        default:
                            valid = false;
                            lex.emit_error(tok.location, static_cast<std::uint32_t>(i), 2, "Invalid escape sequence");
                            break;
                        }

                        if (valid)
                        {
                            static_cast<byte_output*>(currOutput.get())->add_byte(byte);
                        }
                    }
                }
            }
            break;
        }
    }

    if (isScoped)
    {
        // Reached the end without hitting a closing curly
        lex.emit_error(lex.peek().location, "Expected '}}'");
    }

    if (currOutput && !currOutput->is_empty())
    {
        parent->add_child(std::move(currOutput));
    }

    return true;
}

bool parser::write_to_file(const char* path)
{
    binary_writer writer;
    if (!writer.reset(path))
    {
        return false;
    }

    if (!root->write_output(writer))
    {
        std::println("{}: error: Failed to write to file", path);
        return false;
    }

    return true;
}
