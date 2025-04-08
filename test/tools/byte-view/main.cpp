
#include <cstdio>
#include <print>

void print_usage()
{
    std::println(R"^-^(
USAGE
    byte-view <path>

DESCRIPTION
    Outputs the contents of a file in a format that can be used with the 'bin-write' executable to reproduce the exact
    same file as the input.

ARGUMENTS
    path    The path to the input file.
)^-^");
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::println("ERROR: Expected path to a file");
        return print_usage(), 1;
    }

    FILE* file;
    if (fopen_s(&file, argv[1], "rb") != 0)
    {
        std::println("ERROR: Failed to open file '{}'", argv[1]);
        return print_usage(), 1;
    }

    // Get the file size just for logging
    if (fseek(file, 0, SEEK_END) != 0)
    {
        std::println("ERROR: Failed to seek file");
        return 1;
    }
    auto fileSize = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0)
    {
        std::println("ERROR: Failed to seek file");
        return 1;
    }

    std::println("# File: {}", argv[1]);
    std::println("# Size: {} bytes", fileSize);

    static constexpr const std::size_t buffer_size = 64 * 1024; // 64 KiB
    auto buffer = std::make_unique<std::uint8_t[]>(buffer_size);

    static constexpr std::uint8_t line_size = 32;
    const char* prefix = "";
    std::uint8_t bytesInLastOutput = 0;
    while (true)
    {
        auto len = fread(buffer.get(), 1, buffer_size, file);
        if (len == 0)
        {
            if (feof(file))
            {
                break; // End of file
            }
            else
            {
                std::println("ERROR: Failed to read file");
                return 1;
            }
        }

        auto ptr = buffer.get();
        while (len > 0)
        {
            auto lineSize = std::min<std::size_t>(len, line_size - bytesInLastOutput);
            len -= lineSize;
            bytesInLastOutput += static_cast<std::uint8_t>(lineSize);
            for (std::size_t i = 0; i < lineSize; ++i)
            {
                std::print("{}{:02X}", prefix, *ptr++);
                prefix = " ";
            }

            if (bytesInLastOutput == line_size)
            {
                bytesInLastOutput = 0;
                prefix = "";
                std::println();
            }
        }
    }

    std::println();
}
