
#include <bit>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

template <typename IntT>
std::vector<IntT> read_line_as_array()
{
    std::string line;
    std::getline(std::cin, line);

    auto begin = line.c_str();
    auto end = begin + line.size();
    std::vector<IntT> result;
    while (true)
    {
        // Skip past any non-digits
        while ((begin < end) && !std::isdigit(*begin))
        {
            ++begin;
        }

        if (begin >= end)
        {
            break;
        }

        IntT value;
        auto [ptr, ec] = std::from_chars(begin, end, value);
        if (ec != std::errc{})
        {
            std::printf("ERROR: '%s' is not a valid integer\n", begin);
            std::printf("ERROR: %s\n", std::make_error_code(ec).message().c_str());
            return {};
        }

        result.push_back(value);
        begin = ptr;
    }

    if (result.empty())
    {
        std::printf("ERROR: No values specified\n");
    }

    return result;
}

template <typename IntT>
IntT reverse_bits(IntT value, int bits)
{
    IntT result = 0;
    while (bits)
    {
        --bits;
        result |= (value & 0x01) << bits;
        value >>= 1;
    }

    return result;
}

int main()
{
    std::string line;

    std::printf("Enter the code lengths array: ");
    auto codeLens = read_line_as_array<std::uint8_t>();
    if (codeLens.empty())
    {
        return 1;
    }
    else if (codeLens.size() > 288)
    {
        std::printf("ERROR: Too many code lengths; specified %zu, but max is 288\n", codeLens.size());
        return 1;
    }

    std::printf("Enter the data you wish to encode: ");
    auto data = read_line_as_array<std::uint16_t>();
    if (data.empty())
    {
        return 1;
    }

    // Assign codes to all possible output values
    std::uint8_t codeLenCounts[16] = {};
    for (auto len : codeLens)
    {
        if (len > 15)
        {
            std::printf("ERROR: Code length of %u is invalid; maximum allowed value is 15\n", len);
            return 1;
        }

        ++codeLenCounts[len];
    }

    codeLenCounts[0] = 0;
    std::uint16_t nextCodes[16] = {};
    std::uint16_t nextCode = 0;
    for (std::size_t i = 1; i < std::size(codeLenCounts); ++i)
    {
        nextCode = (nextCode + codeLenCounts[i - 1]) << 1;
        nextCodes[i] = nextCode;
    }

    std::uint16_t codes[288];
    for (std::size_t i = 0; i < codeLens.size(); ++i)
    {
        auto len = codeLens[i];
        if (len > 0)
        {
            codes[i] = nextCodes[len]++;
        }
    }

    // Finally, encode the data
    std::vector<std::uint8_t> output;
    std::uint32_t nextValue = 0;
    int nextValueBits = 0;
    for (auto value : data)
    {
        if (value >= codeLens.size())
        {
            std::printf("ERROR: Output value %u is out of range; max value is %zu\n", value, codeLens.size() - 1);
            return 1;
        }

        auto bits = codeLens[value];
        if (!bits)
        {
            std::printf("ERROR: Output value %u has no code assigned (code length of zero)\n", value);
            return 1;
        }

        auto code = reverse_bits(codes[value], bits);
        nextValue |= code << nextValueBits;
        nextValueBits += bits;

        while (nextValueBits >= 8)
        {
            output.push_back(nextValue & 0xFF);
            nextValue >>= 8;
            nextValueBits -= 8;
        }
    }

    if (nextValueBits)
    {
        assert(nextValueBits < 8); // We should have output any full bytes in the loop above
        output.push_back(nextValue & 0xFF);
    }

    std::printf("\nEncoded data: ");

    const char* prefix = "";
    for (auto value : output)
    {
        std::printf("%s0x%02X", prefix, value);
        prefix = ", ";
    }
    std::printf("\n");
}
