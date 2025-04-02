#ifndef PARSER_H

#include <cstdio>
#include <memory>
#include <vector>

#include "lexer.h"

struct binary_writer
{
    ~binary_writer()
    {
        if (file)
        {
            // We may have unwritten data in 'buffer'
            flush_buffer();
            std::fclose(file);
        }
    }

    bool reset(const char* path) noexcept;
    bool write_bytes(const std::uint8_t* data, std::size_t size) noexcept;
    bool write_bits(const std::uint32_t* data, std::size_t size, std::uint8_t bitsInLast) noexcept;

private:
    void flush_buffer()
    {
        if ((write_index != 0) || (bit_index != 0))
        {
            std::size_t writeSize = write_index;
            if (bit_index != 0)
            {
                ++writeSize; // Serves as a byte align
            }

            // We only use the output buffer when in binary output mode, so flush all data
            std::fwrite(buffer, 1, writeSize, file);
            write_index = bit_index = 0;
        }
    }

    FILE* file = nullptr;
    std::uint8_t buffer[256];
    std::uint8_t write_index = 0;
    std::uint8_t bit_index = 0;
};

struct ast_node
{
    virtual ~ast_node() = default;

    virtual bool write_output(binary_writer& writer) = 0;
};

struct output_node : public ast_node
{
    virtual bool is_empty() const noexcept = 0;
};

// Represents an output stream of bits when in binary output mode
struct binary_output final : public output_node
{
    binary_output()
    {
        // We maintain the invariant that 'next_bit' represents the number of valid bits in the last element of the
        // vector. We therefore need to ensure an empty value at the end when this value reaches zero.
        bits.push_back(0);
    }

    virtual bool write_output(binary_writer& writer) override
    {
        return writer.write_bits(bits.data(), bits.size(), next_bit);
    }

    void add_bits(std::uint32_t value, std::uint8_t count)
    {
        while (count > 0)
        {
            auto bitCount = std::min<std::uint8_t>(count, 32 - next_bit);
            auto mask = static_cast<std::uint32_t>(0xFFFFFFFF) >> (32 - bitCount);

            bits.back() |= (value & mask) << next_bit;
            value >>= bitCount;
            count -= bitCount;
            next_bit = (next_bit + bitCount) % 32;

            if (next_bit == 0)
            {
                // Maintain the invariant
                bits.push_back(0);
            }
        }
    }

    virtual bool is_empty() const noexcept override
    {
        return bits.size() == 1 && next_bit == 0;
    }

private:
    std::uint8_t next_bit = 0;
    std::vector<std::uint32_t> bits;
};

// Represents an output stream of bytes when in byte/word output mode
struct byte_output final : public output_node
{
    virtual bool write_output(binary_writer& writer) override
    {
        return writer.write_bytes(bytes.data(), bytes.size());
    }

    void add_byte(std::uint8_t byte)
    {
        bytes.push_back(byte);
    }

    void add_word(std::uint16_t word)
    {
        bytes.push_back(static_cast<std::uint8_t>(word));
        bytes.push_back(static_cast<std::uint8_t>(word >> 8));
    }

    virtual bool is_empty() const noexcept override
    {
        return bytes.empty();
    }

private:
    std::vector<std::uint8_t> bytes;
};

struct scope : public ast_node
{
    virtual bool write_output(binary_writer& writer) override
    {
        for (auto&& child : children)
        {
            if (!child->write_output(writer))
            {
                return false;
            }
        }

        return true;
    }

    void add_child(std::unique_ptr<ast_node> child)
    {
        children.push_back(std::move(child));
    }

protected:
    std::vector<std::unique_ptr<ast_node>> children;
};

// Represents a 'repeat(count) {}' block
struct repeat final : public scope
{
    repeat(std::uint32_t count) noexcept : count(count)
    {
    }

    virtual bool write_output(binary_writer& writer) override
    {
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (!scope::write_output(writer))
            {
                return false;
            }
        }

        return true;
    }

private:
    std::uint32_t count;
};

struct parser_state
{
    std::uint8_t input_mode = 16; // 2 (binary), 10 (decimal), or 16 (hexadecimal)
    std::uint8_t output_mode = 8; // 1, 8, or 16
    bool reverse_output = false;  // Only valid in binary output mode; bits get "flipped" when written
};

struct parser
{
    parser() = default;

    bool parse(const char* path);
    bool write_to_file(const char* path);

private:
    bool parse_output(scope* parent, bool isScoped = false);

    lexer lex;
    parser_state state;
    std::unique_ptr<scope> root;
};

#endif
