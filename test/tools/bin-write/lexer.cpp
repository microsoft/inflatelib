
#include "lexer.h"

#include <array>
#include <cassert>

lexer::lexer()
{
    input_buffer_capacity = 256;
    input_buffer = std::make_unique<char[]>(input_buffer_capacity);
}

bool lexer::open(const char* path)
{
    if (file != nullptr)
    {
        std::fclose(file); // NOTE: 'file' to be overwritten later

        // Also need to reset state
        input_buffer_size = 0;
        current_line_offset = 0;
        current_line_file_offset = 0;
        current_line = {};
        current_token = {};
        token_consumed = true; // Forces us to always read a new token at the start
    }

    file_path = path;
    if (fopen_s(&file, path, "rb") != 0)
    {
        std::println("{}: error: Failed to open file for reading", path);
        file = nullptr;
        return false;
    }

    // We need to read some data, otherwise we'll confuse ourselves later and think that we've reached the end of the file
    ensure_current_line();

    return true;
}

struct char_table
{
    // Each byte represents 8 boolean values
    std::uint8_t data[32] = {};

    constexpr bool operator[](char ch) const noexcept
    {
        return get(ch);
    }

    constexpr bool get(char ch) const noexcept
    {
        auto index = static_cast<unsigned char>(ch) >> 3;
        auto bit = static_cast<unsigned char>(ch) & 0x07;
        return (data[index] & (0x01 << bit)) != 0;
    }

    constexpr void set(char ch, bool value = true) noexcept
    {
        auto index = static_cast<unsigned char>(ch) >> 3;
        auto bit = static_cast<unsigned char>(ch) & 0x07;
        if (value)
        {
            data[index] |= (0x01 << bit);
        }
        else
        {
            data[index] &= ~(0x01 << bit);
        }
    }
};

static constexpr char_table whitespace_table() noexcept
{
    char_table table;
    table.set(' ');
    table.set('\t');
    table.set('\v');
    table.set('\f');
    table.set('\r');
    table.set('\n');
    return table;
}

static constexpr bool is_whitespace(char ch) noexcept
{
    constexpr const auto table = whitespace_table();
    return table[ch];
}

static constexpr char_table alphanumeric_table() noexcept
{
    char_table table;
    for (char ch = '0'; ch <= '9'; ++ch)
    {
        table.set(ch);
    }
    for (char ch = 'A'; ch <= 'Z'; ++ch)
    {
        table.set(ch);
    }
    for (char ch = 'a'; ch <= 'z'; ++ch)
    {
        table.set(ch);
    }
    return table;
}

static constexpr bool is_alphanumeric(char ch) noexcept
{
    constexpr const auto table = alphanumeric_table();
    return table[ch];
}

token lexer::peek()
{
    if (token_consumed)
    {
        token_consumed = false;

        // Advance past the current token
        auto index = current_token.location.column + current_token.text.size() - 1;

        while (true)
        {
            // Skip past any whitespace
            while (index < current_line.size() && is_whitespace(current_line[index]))
            {
                ++index;
            }

            if (index < current_line.size())
            {
                if (current_line[index] != '#')
                {
                    // Not a comment; must be the start of the next token
                    current_token.location.column = static_cast<std::uint32_t>(index + 1);
                    current_token.location.file_offset = current_line_file_offset + index;
                    read_token_at_current_position();
                    break; // ... out of the loop & return the current token
                }
                // Otherwise the start of a comment; we want to skip the rest of the line, which will happen below
            }

            // If we get this far, it's because we've exhausted the current line (no more data or hit a comment)
            if (!advance_line())
            {
                // EOF
                current_token.kind = token_kind::eof;
                current_token.text = {};
                break;
            }

            index = 0; // Start at the beginning of the new line
        }
    }

    return current_token;
}

token lexer::next()
{
    peek();
    token_consumed = true;
    return current_token;
}

std::string_view lexer::read_alphanumeric(std::size_t lineOffset)
{
    std::size_t end = lineOffset;
    while ((end < current_line.size()) && is_alphanumeric(current_line[end]))
    {
        ++end;
    }

    return std::string_view(current_line.data() + lineOffset, end - lineOffset);
}

void lexer::read_token_at_current_position()
{
    auto index = current_token.location.column - 1;
    if (index >= current_line.size())
    {
        assert((index == current_line.size()) && std::feof(file));
        current_token.kind = token_kind::eof;
        current_token.text = {};
        return;
    }

    switch (current_line[index])
    {
    case '>':
    {
        auto numericIndex = index + 1;
        bool doubleShift = false;
        if ((numericIndex < current_line.size()) && (current_line[numericIndex] == '>'))
        {
            ++numericIndex;
            doubleShift = true;
        }

        // Changing the output mode. Expect '>1', '>>1', '>8', or '>16'
        auto id = read_alphanumeric(numericIndex);
        current_token.text = current_line.substr(index, id.size() + (numericIndex - index));
        if (id == "1")
        {
            current_token.kind = doubleShift ? token_kind::output_binary_reversed : token_kind::output_binary;
        }
        else if (doubleShift)
        {
            current_token.kind = token_kind::invalid;
            emit_error(current_token.location, "Invalid output mode");
            if (id.empty())
            {
                std::println(
                    "{}({},{}): note: There should not be any space(s) following '>'",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
            else
            {
                std::println(
                    "{}({},{}): note: Only binary output mode supports reversing ('>>1')",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
        }
        else if (id == "8")
        {
            current_token.kind = token_kind::output_byte;
        }
        else if (id == "16")
        {
            current_token.kind = token_kind::output_word;
        }
        else
        {
            current_token.kind = token_kind::invalid;
            emit_error(current_token.location, "Invalid output mode");
            if (id.empty())
            {
                std::println(
                    "{}({},{}): note: There should not be any space(s) following '>'",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
            else
            {
                std::println(
                    "{}({},{}): note: Expected an output mode of '1', '8', or '16'",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
        }
        break;
    }

    case '<':
    {
        // Changing the input mode. Expect '<bin', '<dec', or '<hex'
        auto id = read_alphanumeric(index + 1);
        current_token.text = current_line.substr(index, id.size() + 1);
        if (id == "bin")
        {
            current_token.kind = token_kind::input_binary;
        }
        else if (id == "dec")
        {
            current_token.kind = token_kind::input_decimmal;
        }
        else if (id == "hex")
        {
            current_token.kind = token_kind::input_hexadecimal;
        }
        else
        {
            current_token.kind = token_kind::invalid;
            emit_error(current_token.location, "Invalid input mode");
            if (id.empty())
            {
                std::println(
                    "{}({},{}): note: There should not be any space(s) following '<'",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
            else
            {
                std::println(
                    "{}({},{}): note: Expected an input mode of 'bin', 'dec', or 'hex'",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
            }
        }
        break;
    }

    case '.':
    {
        // Elipsis. Expect '...'
        auto end = index + 1;
        while ((end < current_line.size()) && current_line[end] == '.')
        {
            ++end;
        }

        auto len = end - index;
        current_token.text = current_line.substr(index, len);
        if (len == 3)
        {
            current_token.kind = token_kind::elipsis;
        }
        else
        {
            current_token.kind = token_kind::invalid;
            emit_error(current_token.location, "Invalid token");
            std::println(
                "{}({},{}): note: Did you mean an ellipsis ('...')?",
                file_path,
                current_token.location.line,
                current_token.location.column);
        }
        break;
    }

    case '(':
        // This is the whole token & there cannot be an error
        current_token.kind = token_kind::paren_open;
        current_token.text = current_line.substr(index, 1);
        break;

    case ')':
        // This is the whole token & there cannot be an error
        current_token.kind = token_kind::paren_close;
        current_token.text = current_line.substr(index, 1);
        break;

    case '{':
        // This is the whole token & there cannot be an error
        current_token.kind = token_kind::curly_open;
        current_token.text = current_line.substr(index, 1);
        break;

    case '}':
        // This is the whole token & there cannot be an error
        current_token.kind = token_kind::curly_close;
        current_token.text = current_line.substr(index, 1);
        break;

    case '"':
    {
        // String. Expect a sequence of characters followed by a terminating '"'
        std::size_t end = index + 1;
        while (true)
        {
            end = current_line.find_first_of("\"\\", end);
            if (end == current_line.npos)
            {
                current_token.kind = token_kind::invalid;
                current_token.text = current_line.substr(index);
                emit_error(current_token.location, "Invalid token");
                std::println(
                    "{}({},{}): note: Expected a terminating '\"' character",
                    file_path,
                    current_token.location.line,
                    current_token.location.column);
                break;
            }
            else if (current_line[end] == '"')
            {
                break; // Found the end of the string
            }
            else
            {
                // Escape sequence. Assume valid until proven otherwise later
                assert(current_line[end] == '\\');
                end += 2;
            }
        }

        if (end != current_line.npos)
        {
            current_token.text = current_line.substr(index, end - index + 1);
            current_token.kind = token_kind::string;
        }

        break;
    }

    default:
    {
        // Expect either "repeat" or a numeric value (inclusive of hexadecimal). Since what counts as a valid numeric
        // value depends on the input mode, we delay validation until later in the parser
        current_token.text = read_alphanumeric(index);
        if (current_token.text.empty())
        {
            current_token.kind = token_kind::invalid;
            current_token.text = current_line.substr(index, 1);
            emit_error(current_token.location, "Invalid token");
        }
        else if (current_token.text == "repeat")
        {
            current_token.kind = token_kind::kw_repeat;
        }
        else
        {
            // Could still be invalid, but the lexer doesn' have enough info to know yet
            current_token.kind = token_kind::value;
        }
    }
    }
}

void lexer::seek_to(const source_location& loc)
{
    // The simplest case is if we're still on the same line
    if (loc.line == current_token.location.line)
    {
        if (loc.column != current_token.location.column)
        {
            assert(loc.column <= current_line.size()); // Otherwise corrupt location
            current_token.location = loc;
            read_token_at_current_position();
        }
        // Otherwise this is the current token; don't waste our time re-reading it
        return;
    }

    // Second simplest case is if the full line is in 'input_buffer'. Since we don't yet know where the end of the line
    // is, this case generalizes to the case where the start of the line is in 'input_buffer'
    auto lineFileOffset = loc.file_offset - (loc.column - 1);
    auto bufferStartFileOffset = current_line_file_offset - current_line_offset;
    auto bufferEndFileOffset = bufferStartFileOffset + input_buffer_size;
    if ((lineFileOffset >= bufferStartFileOffset) && (lineFileOffset < bufferEndFileOffset))
    {
        current_line_file_offset = lineFileOffset;
        current_line_offset = static_cast<std::size_t>(lineFileOffset - bufferStartFileOffset);
        current_token.location = loc;
        ensure_current_line();
        read_token_at_current_position();
        return;
    }

    // Otherwise, we need to seek to the file position
    if (std::fseek(file, static_cast<long>(lineFileOffset), SEEK_SET))
    {
        std::println("{}: error: Failed to seek to line {}", file_path, loc.line);
        std::exit(1);
    }

    // Because we just did a file seek, none of our buffer data is correct
    input_buffer_size = 0;
    current_line_offset = 0;
    current_line_file_offset = lineFileOffset;
    current_token.location = loc;
    ensure_current_line();
    read_token_at_current_position();
}

bool lexer::advance_line()
{
    current_line_offset += current_line.size();
    current_line_file_offset += current_line.size();

    if (current_line_offset < input_buffer_size)
    {
        // Must be a new line (which might be CRLF); skip past the newline character(s)
        if (input_buffer[current_line_offset] == '\r')
        {
            ++current_line_offset;
            ++current_line_file_offset;
        }

        // We search for '\n', so this must be the next character, even if the previous one was '\r'
        assert((current_line_offset < input_buffer_size) && input_buffer[current_line_offset] == '\n');
        ++current_line_offset;
        ++current_line_file_offset;
        ++current_token.location.line;
        current_token.location.column = 1;
        current_token.location.file_offset = current_line_file_offset;
    }
    else
    {
        // Otherwise, we previously didn't read a new line character, which implies we read all of the file
        assert(std::feof(file));
        current_token.location.file_offset = current_line_file_offset;
        current_token.location.column = static_cast<std::uint32_t>(current_line.size() + 1);
        return false;
    }

    ensure_current_line();
    return true;
}

void lexer::ensure_current_line()
{
    // Loop until we find new line character(s) or run out of data
    while (true)
    {
        // First check to see if we already have enough data
        auto lineBegin = input_buffer.get() + current_line_offset;
        auto remainingSize = input_buffer_size - current_line_offset;
        auto pos = reinterpret_cast<char*>(std::memchr(lineBegin, '\n', remainingSize));
        if (pos)
        {
            // New line found; we have enough data already
            auto len = pos - lineBegin;

            // Check for CRLF
            if ((len > 0) && (lineBegin[len - 1] == '\r'))
            {
                --len;
            }

            current_line = std::string_view(lineBegin, len);
            return;
        }

        // We either need to resize the buffer or shift data to the front before reading more data
        if (remainingSize == input_buffer_capacity)
        {
            assert(current_line_offset == 0); // Requirement for the remaining size to be the full buffer capacity
            assert(input_buffer_size == input_buffer_capacity); // Requirement for remaining size to be this large
            auto newCapacity = input_buffer_capacity * 2;
            auto newBuffer = std::make_unique<char[]>(newCapacity);
            std::memcpy(newBuffer.get(), lineBegin, remainingSize);
            input_buffer = std::move(newBuffer);
            input_buffer_capacity = newCapacity;
            // Continue below to read more data
        }
        else
        {
            // Move data to the front of the buffer to make room for more data
            std::memmove(input_buffer.get(), lineBegin, remainingSize); // memmove because there might be overlap
        }

        // NOTE: At this point, 'lineBegin' is incorrect, so from here until the end of the loop scope we cannot use it
        // Similarly, 'input_buffer_size' is incorrect until we update it below
        assert(input_buffer_capacity > remainingSize); // Should be handled by the resize
        current_line_offset = 0;                       // We moved any valid data to the front of the buffer

        auto bytesRead = std::fread(input_buffer.get() + remainingSize, 1, input_buffer_capacity - remainingSize, file);
        input_buffer_size = remainingSize + bytesRead;
        if (!bytesRead)
        {
            // Either EOF or failure
            if (std::feof(file))
            {
                // All data in the buffer is the last line
                current_line = std::string_view(input_buffer.get(), input_buffer_size);
                return;
            }
            else
            {
                assert(std::ferror(file)); // Otherwise 'fread' should have given us a positive value
                std::println("{}: error: Failed to read from file", file_path);
                std::exit(1);
            }
        }
        // Otherwise loop to see if we now have a new line character
    }
}
