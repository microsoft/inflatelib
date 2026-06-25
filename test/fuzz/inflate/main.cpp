/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

#include <inflatelib.h>

#ifdef _WIN32
#define FUZZ_EXPORT __declspec(dllexport)
#define FUZZ_CALLCONV __cdecl
#else
#define FUZZ_EXPORT
#define FUZZ_CALLCONV
#endif

extern "C" FUZZ_EXPORT int FUZZ_CALLCONV LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
try
{
    // To help make fuzzing results more useful, we take input/output buffer size(s) as arguments so that we exercise a
    // larger percentage of the code paths.
    auto calculateBufferSizes = [&](std::vector<std::size_t>& sizes, std::size_t& maxSize) -> bool {
        if (!size)
        {
            return false; // Empty input or already consumed everything
        }

        std::size_t count = (*data++) + 1; // Add 1 because it doesn't make sense to have zero buffer sizes
        auto bytes = count * 2;            // We use 16 bit sizes
        if (bytes > --size)
        {
            return false;
        }

        // Combine adjacent bytes into 16-bit sizes
        sizes.insert_range(
            sizes.begin(), std::ranges::subrange(data, data + bytes) | std::views::chunk(2) | std::views::transform([](auto chunk) {
                               auto result = static_cast<std::size_t>(chunk[0]); // Little-endian
                               result |= (static_cast<std::size_t>(chunk[1]) << 8);
                               return result + 1; // Add 1 because it doesn't make sense to have zero buffer sizes
                           }));
        data += bytes;
        size -= bytes;

        maxSize = std::ranges::max(sizes);

        return true;
    };
    std::vector<std::size_t> inputBufferSizes, outputBufferSizes;
    std::size_t maxInputBufferSize, maxOutputBufferSize;
    if (!calculateBufferSizes(inputBufferSizes, maxInputBufferSize) || !calculateBufferSizes(outputBufferSizes, maxOutputBufferSize))
    {
        return -1;
    }

    auto inputBuffer = std::make_unique<uint8_t[]>(maxInputBufferSize);
    auto outputBuffer = std::make_unique<uint8_t[]>(maxOutputBufferSize);

    inflatelib_stream stream = {};
    if (inflatelib_init(&stream) < INFLATELIB_OK)
    {
        return -1;
    }

    int result = 0;
    for (std::size_t i = 0; size > 0; ++i)
    {
        auto inputBufferSize = std::min(size, inputBufferSizes[i % inputBufferSizes.size()]);
        auto inputBufferPtr = inputBuffer.get() + (maxInputBufferSize - inputBufferSize);
        std::memcpy(inputBufferPtr, data, inputBufferSize);
        stream.next_in = inputBufferPtr;
        stream.avail_in = inputBufferSize;

        auto outputBufferSize = outputBufferSizes[i % outputBufferSizes.size()];
        auto outputBufferPtr = outputBuffer.get() + (maxOutputBufferSize - outputBufferSize);
        stream.next_out = outputBufferPtr;
        stream.avail_out = outputBufferSize;

        auto inflateResult = inflatelib_inflate(&stream);
        if (inflateResult == INFLATELIB_EOF)
        {
            break;
        }
        else if (inflateResult < INFLATELIB_OK)
        {
            result = -1;
            break;
        }

        auto bytesConsumed = static_cast<const uint8_t*>(stream.next_in) - inputBufferPtr;
        data += bytesConsumed;
        size -= bytesConsumed;
    }

    inflatelib_destroy(&stream);

    return result;
}
catch (...)
{
    return -1;
}
