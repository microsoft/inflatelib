/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <bit>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <print>
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
            std::println("ERROR: '{}' is not a valid integer", begin);
            std::println("ERROR: {}", std::make_error_code(ec).message().c_str());
            return {};
        }

        result.push_back(value);
        begin = ptr;
    }

    if (result.empty())
    {
        std::println("ERROR: No values specified");
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

    std::print("Enter the code lengths array: ");
    auto codeLens = read_line_as_array<std::uint8_t>();
    if (codeLens.empty())
    {
        return 1;
    }
    else if (codeLens.size() > 288)
    {
        std::println("ERROR: Too many code lengths; specified {}, but max is 288", codeLens.size());
        return 1;
    }

    std::print("Enter the data you wish to encode: ");
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
            std::println("ERROR: Code length of {} is invalid; maximum allowed value is 15", len);
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
            std::println("ERROR: Output value {} is out of range; max value is {}", value, codeLens.size() - 1);
            return 1;
        }

        auto bits = codeLens[value];
        if (!bits)
        {
            std::println("ERROR: Output value {} has no code assigned (code length of zero)", value);
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

    std::print("\nEncoded data: ");

    const char* prefix = "";
    for (auto value : output)
    {
        std::print("{}0x{:02X}", prefix, value);
        prefix = ", ";
    }
    std::println("");
}
