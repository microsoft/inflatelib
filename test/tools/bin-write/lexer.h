#ifndef LEXER_H

#include <cstdint>
#include <cstdio>
#include <memory>
#include <print>
#include <string_view>

enum class token_kind
{
    init,    // Special value to indicate that the lexer has not yet read a token
    eof,     // No more data
    value,   // A numeric value
    invalid, // An invalid token was encountered, however it's possible to skip past it

    // "Keywords"
    kw_repeat, // repeat

    // Output modes
    output_binary,          // >1
    output_binary_reversed, // >>1
    output_byte,            // >8
    output_word,            // >16

    // Input modes
    input_binary,      // <bin
    input_decimmal,    // <dec
    input_hexadecimal, // <hex

    // Misc
    elipsis,     // ...
    paren_open,  // (
    paren_close, // )
    curly_open,  // {
    curly_close, // }
    string,      // "..."
};

struct source_location
{
    std::uint32_t line = 1;
    std::uint32_t column = 1;
    std::uint64_t file_offset = 0;
};

struct token
{
    token_kind kind = token_kind::init;
    std::string_view text;
    source_location location;
};

struct lexer
{
    lexer();

    bool open(const char* path);

    token peek();
    token next();

    // Returns true if it's possible to read another token. Note that this does not mean that an error has not been
    // encountered (e.g. invalid token or parse error)
    operator bool() const noexcept
    {
        return current_token.kind != token_kind::eof;
    }

    template <typename... Args>
    void emit_error(source_location loc, std::uint32_t offset, std::uint32_t len, std::format_string<Args...> fmt, Args&&... args)
    {
        ++error_count;
        auto oldLoc = current_token.location;
        seek_to(loc);

        len = std::min(len, static_cast<std::uint32_t>(current_token.text.size() - offset));

        // We assume that the token has already been set up to indicate where the error occurred
        std::print("{}({},{}): error: ", file_path, current_token.location.line, current_token.location.column + offset);
        std::println(fmt, std::forward<Args>(args)...);

        // We output the text info in the format:
        // line |               error-occurred-here
        //      |               ^~~~~~~~~~~~~~~~~~~
        // We assume that 'line' can be up to 99,999 and we want to ensure that we don't output more than 100 characters
        // if we can avoid it
        auto outputBegin = 0;
        auto errBegin = current_token.location.column + offset - 1;
        auto errEnd = errBegin + len;
        if (errEnd > 100)
        {
            // Prefer to shift the output left, so long as we still capture the start of the error
            auto shift = errEnd - 100;
            if (shift > errBegin)
            {
                shift = errBegin;
            }

            outputBegin = shift;
            errBegin -= shift;
            errEnd -= shift;

            if (errEnd > 100)
            {
                // So we don't write too many '~' characters
                errEnd = 100;
            }
        }

        auto line = current_line.substr(outputBegin, 100);
        std::println("{:>5} | {}", current_token.location.line, line);

        // NOTE: This is semi-abusing the format specifiers to do repeat characters for us
        // NOTE: The '+1' for errBegin is to protect against the case where column==1
        std::println("      |{: >{}}{:~<{}}", ' ', errBegin + 1, '^', errEnd - errBegin);

        seek_to(oldLoc);
    }

    template <typename... Args>
    void emit_error(source_location loc, std::format_string<Args...> fmt, Args&&... args)
    {
        emit_error(loc, 0, std::numeric_limits<std::uint32_t>::max(), fmt, std::forward<Args>(args)...);
    }

    bool saw_error() const noexcept
    {
        return error_count != 0;
    }

private:
    std::string_view read_alphanumeric(std::size_t lineOffset);

    // Assumes we've already advaned current_line/current_token, etc. to the start of the token
    void read_token_at_current_position();
    // Assumes 'loc' is the starting location of a valid or invalid token (i.e. not whitespace, inside a comment, etc.)
    void seek_to(const source_location& loc);
    bool advance_line();
    void ensure_current_line();

    std::string file_path;
    std::FILE* file = nullptr;
    token current_token;
    bool token_consumed = true;
    int error_count = 0;

    std::unique_ptr<char[]> input_buffer;
    std::size_t input_buffer_capacity = 0;
    std::size_t input_buffer_size = 0;
    std::size_t current_line_offset = 0;        // Offset to where 'current_line' begins
    std::uint64_t current_line_file_offset = 0; // File offset to the beginning of 'current_line'
    std::string_view current_line;
};

#endif
