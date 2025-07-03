/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <catch.hpp>
#include <span>

#include <internal.h>

using namespace std::literals;

void DoHuffmanTreeTest(
    const uint8_t* codeLengths, uint16_t codeLengthsSize, const uint8_t* input, size_t inputSize, const uint16_t* expectedOutput, size_t outputSize)
{
    // How much data to set on the input stream at a time
    size_t strides[] = {1, 7 /*prime value*/, inputSize};
    for (auto stride : strides)
    {
        // We need an inflatelib_stream, but only for the purposes of input/setting error codes
        inflatelib_stream stream = {};
        REQUIRE(inflatelib_init(&stream) == INFLATELIB_OK);

        huffman_tree tree;
        REQUIRE(huffman_tree_init(&tree, &stream, codeLengthsSize) == INFLATELIB_OK);
        REQUIRE(huffman_tree_reset(&tree, &stream, codeLengths, codeLengthsSize) == INFLATELIB_OK);

        std::vector<std::uint16_t> output;
        for (size_t offset = 0; offset < inputSize;)
        {
            auto len = std::min(stride, inputSize - offset);
            bitstream_set_data(&stream.internal->bitstream, input + offset, len);

            while (output.size() < outputSize) // Make sure we don't try and consume extra bits at the end of input
            {
                std::uint16_t symbol;
                auto result = huffman_tree_lookup(&tree, &stream, &symbol);
                REQUIRE(result >= 0);
                if (result == 0)
                    break; // Need more data

                output.push_back(symbol);
            }

            offset += len;
        }

        huffman_tree_destroy(&tree, &stream);
        inflatelib_destroy(&stream);

        REQUIRE(output.size() == outputSize);
        for (size_t i = 0; i < outputSize; ++i)
        {
            REQUIRE(output[i] == expectedOutput[i]);
        }
    }
}

TEST_CASE("HuffmanTreeCodeLengthTableTests", "[huffman_tree]")
{
    // NOTE: The code lengths codes are encoded using 3 bits (0-7), and these codes represent values 0-18 (19 code
    // lengths in total)

    SECTION("Balanced tree")
    {
        // Representing 19 values, each with an equal number of bits, requires 5 bits
        const uint8_t codeLengths[19] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
                                   17, 16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,
                                   4,  6,  8,  10, 12, 14, 16, 18, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {0x00, 0x22, 0x4C, 0x28, 0xE3, 0x42, 0x2A, 0x6D, 0xAC, 0xF3, 0x21, 0xA6,
                                 0x18, 0xBC, 0xB3, 0x46, 0x2B, 0x29, 0x38, 0xA3, 0x04, 0x23, 0x08, 0x10,
                                 0x61, 0x42, 0x19, 0x17, 0x52, 0xF4, 0x56, 0x4B, 0x4E, 0x31, 0x04};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Unbalanced tree")
    {
        // Represents 19 values in the most unbalanced way possible
        const uint8_t codeLengths[19] = {1, 2, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
                                   17, 16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,
                                   4,  6,  8,  10, 12, 14, 16, 18, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {0xDA, 0xE1, 0x78, 0x3A, 0x5F, 0xAE, 0xB7, 0xFB, 0xE3, 0xF9, 0x7A, 0x7F, 0xBE, 0xBF, 0xFF,
                                 0xEF, 0xFB, 0x79, 0xBF, 0x9E, 0x8F, 0xFB, 0xED, 0x7A, 0x39, 0x9F, 0x8E, 0x87, 0x65, 0xC7,
                                 0xF3, 0xF5, 0xFE, 0x7C, 0x7F, 0xFF, 0xBF, 0xCF, 0xEB, 0x71, 0xBB, 0x9C, 0x0E, 0x01};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Sparse tree")
    {
        // sparse in the sense that only some values (even values) have codes
        const uint8_t codeLengths[19] = {3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 3, 0, 4, 0, 4, 0, 4, 0, 4};

        const uint16_t output[] = {0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 16, 14, 12, 10, 8,  6,  4,  2,  0,
                                   2,  2,  4,  4,  4,  6,  6,  6,  6,  8,  8,  8,  8,  8,  10, 10, 10, 10, 10,
                                   10, 12, 12, 12, 12, 12, 12, 12, 14, 14, 14, 14, 14, 14, 14, 14, 16, 16, 16,
                                   16, 16, 16, 16, 16, 16, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
        const uint8_t input[] = {0xA0, 0x9C, 0xCE, 0xDE, 0xDF, 0x4E, 0x63, 0x11, 0xA4, 0x24, 0xDB,
                                 0x4E, 0x92, 0xB4, 0x6D, 0x3B, 0x33, 0x33, 0x33, 0xBB, 0xBB, 0xBB,
                                 0xBB, 0x77, 0x77, 0x77, 0x77, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Tall tree")
    {
        // Same as the balanced tree, only with a max height (7 bits per code)
        const uint8_t codeLengths[19] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
                                   17, 16, 15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,
                                   4,  6,  8,  10, 12, 14, 16, 18, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {0x00, 0x20, 0x08, 0x0C, 0x81, 0xC2, 0xE0, 0x08, 0x24, 0x0A, 0x8D, 0xC1, 0xE2,
                                 0xF0, 0x04, 0x22, 0x89, 0x48, 0xC0, 0xE3, 0xB0, 0x18, 0x34, 0x0A, 0x89, 0x80,
                                 0xC3, 0xA0, 0x10, 0x30, 0x08, 0x08, 0x00, 0x41, 0x60, 0x08, 0x14, 0x06, 0x47,
                                 0x20, 0x11, 0xF1, 0x58, 0x34, 0x12, 0x0E, 0x05, 0x03, 0x01};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Short tree")
    {
        // Like the sparse tree, but REALLY sparse; only two entries with a single bit code size
        const uint8_t codeLengths[19] = {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0};

        const uint16_t output[] = {6,  15, 6, 6,  15, 6, 15, 15, 15, 15, 6,  6, 6,  15, 6, 6,  6, 6,  6,  15, 6,  15, 6, 15,
                                   15, 15, 6, 15, 15, 6, 6,  6,  6,  6,  15, 6, 15, 6,  6, 15, 6, 15, 15, 15, 15, 6,  15};
        const uint8_t input[] = {0xD2, 0x23, 0xA8, 0x1B, 0x94, 0x5E};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }
}

TEST_CASE("HuffmanTreeDistanceTableTests", "[huffman_tree]")
{
    // NOTE: The distance code lengths are encoded using the code lengths dictionary, which defines a maximum bit count
    // of 15, and these codes represent values 0-31 (32 distance codes in total)

    SECTION("Balanced tree")
    {
        // Representing 32 values, each with an equal number of bits, requires 5 bits
        const uint8_t codeLengths[32] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                                         5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24, 25, 26, 27, 28, 29, 30, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                   14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,  4,  6,  8,  10, 12, 14, 16, 18,
                                   20, 22, 24, 26, 28, 30, 31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {0x00, 0x22, 0x4C, 0x28, 0xE3, 0x42, 0x2A, 0x6D, 0xAC, 0xF3, 0x21, 0xA6, 0x5C, 0x6A, 0xEB,
                                 0x63, 0xAE, 0x7D, 0xEE, 0xFB, 0xEF, 0x9E, 0xBD, 0xE6, 0xE8, 0xAD, 0x96, 0x9C, 0x62, 0xF0,
                                 0xCE, 0x1A, 0xAD, 0xA4, 0xE0, 0x8C, 0x12, 0x8C, 0x20, 0x40, 0x84, 0x09, 0x65, 0x5C, 0x48,
                                 0xA5, 0x8D, 0x75, 0xDE, 0xBF, 0x7B, 0xF6, 0x9A, 0xA3, 0xB7, 0x5A, 0x72, 0x8A, 0x21};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Unbalanced tree")
    {
        // Represents 32 values in the most unbalanced way possible
        const uint8_t codeLengths[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 12, 13, 15, 15, 15, 15,
                                         15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24, 25, 26, 27, 28, 29, 30, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                   14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,  4,  6,  8,  10, 12, 14, 16, 18,
                                   20, 22, 24, 26, 28, 30, 31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {0xDA, 0xBD, 0xEF, 0xF7, 0xF7, 0xEF, 0xBF, 0xFF, 0xF9, 0x5F, 0xFF, 0x9B, 0xFF, 0xED, 0xFF, 0xEE,
                                 0x7F, 0xFF, 0x7F, 0xF8, 0x3F, 0xFE, 0x9F, 0xFE, 0xCF, 0xFF, 0x97, 0xFF, 0xEB, 0xFF, 0xED, 0xFF,
                                 0xFE, 0xFF, 0xF8, 0x7F, 0xFE, 0xBF, 0xFE, 0xDF, 0xFF, 0x9F, 0xFF, 0xEF, 0xFF, 0xEF, 0xFF, 0xFF,
                                 0xFF, 0xFB, 0xFF, 0xFE, 0x7F, 0xFE, 0xDF, 0xFF, 0xAF, 0xFF, 0xE7, 0xFF, 0xE3, 0xFF, 0xFE, 0x7F,
                                 0xFB, 0xBF, 0xFE, 0x5F, 0xFE, 0xCF, 0xFF, 0xA7, 0xFF, 0xE3, 0xFF, 0xE1, 0x7F, 0xFF, 0xBF, 0xFB,
                                 0xDF, 0xFE, 0x6F, 0xFE, 0xD7, 0xFF, 0xFC, 0xF7, 0xEF, 0xEF, 0xF7, 0xBD, 0x5B, 0xF6, 0x7E, 0xFF,
                                 0xFE, 0xE7, 0x7F, 0xF3, 0xBF, 0xFB, 0x3F, 0xFC, 0x9F, 0xFE, 0x2F, 0xFF, 0xB7, 0xFF, 0xC7, 0xFF,
                                 0xEB, 0xFF, 0xF3, 0xFF, 0xFB, 0xFF, 0xFF, 0x7F, 0xFF, 0xDF, 0xFF, 0xCF, 0xFF, 0xFB, 0xFF, 0xF5,
                                 0xFF, 0xFC, 0x7F, 0xFC, 0xDF, 0xFF, 0x6F, 0xFF, 0xD7, 0x7F, 0x7F, 0xDF, 0x05};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Sparse tree")
    {
        // sparse in the sense that only some values (even values) have codes
        const uint8_t codeLengths[32] = {4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0,
                                         4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0};

        const uint16_t output[] = {0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 28, 26, 24, 22, 20, 18, 16, 14,
                                   12, 10, 8,  6,  4,  2,  0,  2,  2,  4,  4,  4,  6,  6,  6,  6,  8,  8,  8,  8,  8,  10, 10, 10,
                                   10, 10, 10, 12, 12, 12, 12, 12, 12, 12, 14, 14, 14, 14, 14, 14, 14, 14, 16, 16, 16, 16, 16, 16,
                                   16, 16, 16, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
                                   22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
                                   24, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28,
                                   28, 28, 28, 28, 28, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30};
        const uint8_t input[] = {0x80, 0xC4, 0xA2, 0xE6, 0x91, 0xD5, 0xB3, 0xF7, 0xB7, 0xD3, 0x95, 0xE1, 0xA6, 0xC2,
                                 0x84, 0x80, 0x48, 0x44, 0xCC, 0xCC, 0x22, 0x22, 0xA2, 0xAA, 0xAA, 0x6A, 0x66, 0x66,
                                 0x66, 0xEE, 0xEE, 0xEE, 0xEE, 0x11, 0x11, 0x11, 0x11, 0x91, 0x99, 0x99, 0x99, 0x99,
                                 0x59, 0x55, 0x55, 0x55, 0x55, 0x55, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0x33, 0x33,
                                 0x33, 0x33, 0x33, 0x33, 0xB3, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0x7B, 0x77, 0x77,
                                 0x77, 0x77, 0x77, 0x77, 0x77, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Tall tree")
    {
        // Same as the balanced tree, only with a max height (15 bits per code)
        const uint8_t codeLengths[32] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                                         15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24, 25, 26, 27, 28, 29, 30, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                   14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,  4,  6,  8,  10, 12, 14, 16, 18,
                                   20, 22, 24, 26, 28, 30, 31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {
            0x00, 0x00, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x01, 0x80, 0x02, 0xC0, 0x00, 0xE0, 0x00, 0x08, 0x00,
            0x24, 0x00, 0x0A, 0x00, 0x0D, 0x80, 0x01, 0xC0, 0x02, 0xE0, 0x00, 0xF0, 0x00, 0x04, 0x00, 0x22, 0x00, 0x09,
            0x80, 0x0C, 0x40, 0x01, 0xA0, 0x02, 0xD0, 0x00, 0xE8, 0x00, 0x0C, 0x00, 0x26, 0x00, 0x0B, 0x80, 0x0D, 0xC0,
            0x01, 0xE0, 0x02, 0xF0, 0x00, 0xF8, 0x00, 0x3C, 0x00, 0x2E, 0x00, 0x07, 0x80, 0x0D, 0xC0, 0x02, 0x60, 0x02,
            0x30, 0x00, 0xE8, 0x00, 0x34, 0x00, 0x2A, 0x00, 0x05, 0x80, 0x0C, 0x40, 0x02, 0x20, 0x02, 0x10, 0x00, 0xF0,
            0x00, 0x38, 0x00, 0x2C, 0x00, 0x06, 0x00, 0x0D, 0x80, 0x02, 0x40, 0x02, 0x20, 0x00, 0xE0, 0x00, 0x30, 0x00,
            0x28, 0x00, 0x04, 0x00, 0x0C, 0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x40, 0x00, 0x10, 0x00, 0x18, 0x00, 0x02,
            0x00, 0x05, 0x80, 0x01, 0xC0, 0x01, 0x10, 0x00, 0x48, 0x00, 0x14, 0x00, 0x1A, 0x00, 0x03, 0x80, 0x05, 0xC0,
            0x01, 0xE0, 0x01, 0xF0, 0x01, 0xB8, 0x00, 0x6C, 0x00, 0x26, 0x00, 0x1D, 0x80, 0x0A, 0x40, 0x06, 0x20, 0x02,
            0xE0, 0x01, 0xB0, 0x00, 0x68, 0x00, 0x24, 0x00, 0x1C, 0x00, 0x0A, 0x00, 0x06, 0x00, 0x02};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Short tree")
    {
        // Like the sparse tree, but REALLY sparse; only two entries with a single bit code size
        const uint8_t codeLengths[32] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};

        const uint16_t output[] = {22, 22, 8,  22, 8,  8,  8,  8,  8, 22, 8,  22, 22, 22, 22, 8,  22, 22,
                                   8,  8,  22, 8,  22, 8,  8,  8,  8, 8,  22, 8,  8,  8,  22, 22, 8,  8,
                                   8,  8,  8,  22, 22, 22, 22, 22, 8, 22, 8,  22, 22, 8,  22, 8,  8,  8};
        const uint8_t input[] = {0x0B, 0x7A, 0x53, 0x10, 0x83, 0xAF, 0x05};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Memory Usage")
    {
        // Constructs a tree with the goal that said tree consumes the most memory possible. This is accomplished by
        // having two sub-trees: one with 31 leaf nodes (and no "dead" nodes; 60 nodes in total), and one with a single
        // leaf node at max height (8 nodes + 8 "dead" nodes = 16 nodes in total). This gives 76 memory locations needed
        // in total for the binary tree portion of the array.
        const uint8_t codeLengths[32] = {8,  9,  10, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                                         15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                   24, 25, 26, 27, 28, 29, 30, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                   14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0,  2,  4,  6,  8,  10, 12, 14, 16, 18,
                                   20, 22, 24, 26, 28, 30, 31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11, 9,  7,  5,  3,  1};
        const uint8_t input[] = {
            0x00, 0x80, 0x00, 0x03, 0x1C, 0x80, 0x13, 0xE0, 0x0C, 0x70, 0x0E, 0xB8, 0x00, 0x5C, 0x02, 0xAE, 0x00, 0xD7, 0x80,
            0x1B, 0xC0, 0x2D, 0xE0, 0x0E, 0x70, 0x0F, 0x78, 0x00, 0x3C, 0x02, 0x9E, 0x00, 0xCF, 0x80, 0x17, 0xC0, 0x2B, 0xE0,
            0x0D, 0xF0, 0x0E, 0xF8, 0x00, 0x7C, 0x02, 0xBE, 0x00, 0xDF, 0x80, 0x1F, 0xC0, 0x2F, 0xE0, 0x0F, 0xF0, 0x0F, 0x04,
            0x00, 0xFC, 0x03, 0xFE, 0x00, 0xBF, 0x80, 0x1F, 0xC0, 0x37, 0xE0, 0x0B, 0xF0, 0x09, 0xF8, 0x00, 0xBC, 0x03, 0xDE,
            0x00, 0xAF, 0x80, 0x17, 0xC0, 0x33, 0xE0, 0x09, 0xF0, 0x08, 0x78, 0x00, 0xDC, 0x03, 0xEE, 0x00, 0xB7, 0x80, 0x1B,
            0xC0, 0x35, 0xE0, 0x0A, 0x70, 0x09, 0xB8, 0x00, 0x9C, 0x03, 0xCE, 0x00, 0x27, 0xC0, 0x01, 0x18, 0x20, 0x00, 0xC0,
            0x00, 0x27, 0xC0, 0x39, 0xE0, 0x12, 0x70, 0x0D, 0xB8, 0x05, 0xDC, 0x03, 0x1E, 0x01, 0xCF, 0x80, 0x57, 0xC0, 0x3B,
            0xE0, 0x13, 0xF0, 0x0D, 0xF8, 0x05, 0xFC, 0x03, 0x01, 0x00, 0x7F, 0x80, 0x1F, 0xC0, 0x17, 0xE0, 0x03, 0xF0, 0x06,
            0x78, 0x01, 0x3C, 0x01, 0x1E, 0x00, 0x77, 0x80, 0x1B, 0xC0, 0x15, 0xE0, 0x02, 0x70, 0x06, 0x38, 0x00, 0x01};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }
}

TEST_CASE("HuffmanTreeLiteralLengthTableTests", "[huffman_tree]")
{
    // NOTE: The literal/length code lengths are encoded using the code lengths dictionary, which defines a maximum bit
    // count of 15, and these codes represent values 0-287 (288 distance codes in total)

    SECTION("Balanced tree")
    {
        // Representing 288 values, each with an equal number of bits, requires 9 bits
        const uint8_t codeLengths[288] = {
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
            9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};

        const uint16_t output[] = {
            0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
            23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,
            46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,
            69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
            92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
            115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
            138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
            161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
            184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
            207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
            230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252,
            253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275,
            276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287};
        const uint8_t input[] = {
            0x00, 0x00, 0x02, 0x02, 0x0C, 0x04, 0x28, 0x30, 0xE0, 0x20, 0x40, 0x82, 0x02, 0x0D, 0x06, 0x2C, 0x38, 0xF0,
            0x10, 0x20, 0x42, 0x82, 0x0C, 0x05, 0x2A, 0x34, 0xE8, 0x30, 0x60, 0xC2, 0x82, 0x0D, 0x07, 0x2E, 0x3C, 0xF8,
            0x08, 0x10, 0x22, 0x42, 0x8C, 0x04, 0x29, 0x32, 0xE4, 0x28, 0x50, 0xA2, 0x42, 0x8D, 0x06, 0x2D, 0x3A, 0xF4,
            0x18, 0x30, 0x62, 0xC2, 0x8C, 0x05, 0x2B, 0x36, 0xEC, 0x38, 0x70, 0xE2, 0xC2, 0x8D, 0x07, 0x2F, 0x3E, 0xFC,
            0x04, 0x08, 0x12, 0x22, 0x4C, 0x84, 0x28, 0x31, 0xE2, 0x24, 0x48, 0x92, 0x22, 0x4D, 0x86, 0x2C, 0x39, 0xF2,
            0x14, 0x28, 0x52, 0xA2, 0x4C, 0x85, 0x2A, 0x35, 0xEA, 0x34, 0x68, 0xD2, 0xA2, 0x4D, 0x87, 0x2E, 0x3D, 0xFA,
            0x0C, 0x18, 0x32, 0x62, 0xCC, 0x84, 0x29, 0x33, 0xE6, 0x2C, 0x58, 0xB2, 0x62, 0xCD, 0x86, 0x2D, 0x3B, 0xF6,
            0x1C, 0x38, 0x72, 0xE2, 0xCC, 0x85, 0x2B, 0x37, 0xEE, 0x3C, 0x78, 0xF2, 0xE2, 0xCD, 0x87, 0x2F, 0x3F, 0xFE,
            0x02, 0x04, 0x0A, 0x12, 0x2C, 0x44, 0xA8, 0x30, 0xE1, 0x22, 0x44, 0x8A, 0x12, 0x2D, 0x46, 0xAC, 0x38, 0xF1,
            0x12, 0x24, 0x4A, 0x92, 0x2C, 0x45, 0xAA, 0x34, 0xE9, 0x32, 0x64, 0xCA, 0x92, 0x2D, 0x47, 0xAE, 0x3C, 0xF9,
            0x0A, 0x14, 0x2A, 0x52, 0xAC, 0x44, 0xA9, 0x32, 0xE5, 0x2A, 0x54, 0xAA, 0x52, 0xAD, 0x46, 0xAD, 0x3A, 0xF5,
            0x1A, 0x34, 0x6A, 0xD2, 0xAC, 0x45, 0xAB, 0x36, 0xED, 0x3A, 0x74, 0xEA, 0xD2, 0xAD, 0x47, 0xAF, 0x3E, 0xFD,
            0x06, 0x0C, 0x1A, 0x32, 0x6C, 0xC4, 0xA8, 0x31, 0xE3, 0x26, 0x4C, 0x9A, 0x32, 0x6D, 0xC6, 0xAC, 0x39, 0xF3,
            0x16, 0x2C, 0x5A, 0xB2, 0x6C, 0xC5, 0xAA, 0x35, 0xEB, 0x36, 0x6C, 0xDA, 0xB2, 0x6D, 0xC7, 0xAE, 0x3D, 0xFB,
            0x0E, 0x1C, 0x3A, 0x72, 0xEC, 0xC4, 0xA9, 0x33, 0xE7, 0x2E, 0x5C, 0xBA, 0x72, 0xED, 0xC6, 0xAD, 0x3B, 0xF7,
            0x1E, 0x3C, 0x7A, 0xF2, 0xEC, 0xC5, 0xAB, 0x37, 0xEF, 0x3E, 0x7C, 0xFA, 0xF2, 0xED, 0xC7, 0xAF, 0x3F, 0xFF,
            0x01, 0x02, 0x06, 0x0A, 0x1C, 0x24, 0x68, 0xB0, 0xE0, 0x21, 0x42, 0x86, 0x0A, 0x1D, 0x26, 0x6C, 0xB8, 0xF0,
            0x11, 0x22, 0x46, 0x8A, 0x1C, 0x25, 0x6A, 0xB4, 0xE8, 0x31, 0x62, 0xC6, 0x8A, 0x1D, 0x27, 0x6E, 0xBC, 0xF8};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Unbalanced tree")
    {
        // Represents 288 values in the most unbalanced way possible
        const uint8_t codeLengths[288] = {
            1,  2,  3,  4,  5,  6,  8,  9,  10, 12, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {
            0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
            23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,
            46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,
            69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
            92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
            115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
            138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
            161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
            184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
            207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
            230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252,
            253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275,
            276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287};
        const uint8_t input[] = {
            0xDA, 0xBD, 0xEF, 0xE7, 0xD7, 0x6F, 0xBF, 0xF3, 0xBB, 0xFC, 0xAE, 0xBF, 0x9B, 0xDF, 0xED, 0xEF, 0xEE, 0x77, 0xFF,
            0x7B, 0xF8, 0x3D, 0xFE, 0x9E, 0x7E, 0xCF, 0xBF, 0x97, 0xDF, 0xEB, 0xEF, 0xED, 0xF7, 0xFE, 0xFB, 0xF8, 0x7D, 0xFE,
            0xBE, 0x7E, 0xDF, 0xBF, 0x9F, 0xDF, 0xEF, 0xEF, 0xEF, 0xF7, 0xFF, 0x07, 0xF8, 0x03, 0xFE, 0x81, 0xFE, 0xC0, 0x7F,
            0x90, 0x3F, 0xE8, 0x1F, 0xEC, 0x0F, 0xFE, 0x87, 0xF8, 0x43, 0xFE, 0xA1, 0xFE, 0xD0, 0x7F, 0x98, 0x3F, 0xEC, 0x1F,
            0xEE, 0x0F, 0xFF, 0x47, 0xF8, 0x23, 0xFE, 0x91, 0xFE, 0xC8, 0x7F, 0x94, 0x3F, 0xEA, 0x1F, 0xED, 0x8F, 0xFE, 0xC7,
            0xF8, 0x63, 0xFE, 0xB1, 0xFE, 0xD8, 0x7F, 0x9C, 0x3F, 0xEE, 0x1F, 0xEF, 0x8F, 0xFF, 0x27, 0xF8, 0x13, 0xFE, 0x89,
            0xFE, 0xC4, 0x7F, 0x92, 0x3F, 0xE9, 0x9F, 0xEC, 0x4F, 0xFE, 0xA7, 0xF8, 0x53, 0xFE, 0xA9, 0xFE, 0xD4, 0x7F, 0x9A,
            0x3F, 0xED, 0x9F, 0xEE, 0x4F, 0xFF, 0x67, 0xF8, 0x33, 0xFE, 0x99, 0xFE, 0xCC, 0x7F, 0x96, 0x3F, 0xEB, 0x9F, 0xED,
            0xCF, 0xFE, 0xE7, 0xF8, 0x73, 0xFE, 0xB9, 0xFE, 0xDC, 0x7F, 0x9E, 0x3F, 0xEF, 0x9F, 0xEF, 0xCF, 0xFF, 0x17, 0xF8,
            0x0B, 0xFE, 0x85, 0xFE, 0xC2, 0x7F, 0x91, 0xBF, 0xE8, 0x5F, 0xEC, 0x2F, 0xFE, 0x97, 0xF8, 0x4B, 0xFE, 0xA5, 0xFE,
            0xD2, 0x7F, 0x99, 0xBF, 0xEC, 0x5F, 0xEE, 0x2F, 0xFF, 0x57, 0xF8, 0x2B, 0xFE, 0x95, 0xFE, 0xCA, 0x7F, 0x95, 0xBF,
            0xEA, 0x5F, 0xED, 0xAF, 0xFE, 0xD7, 0xF8, 0x6B, 0xFE, 0xB5, 0xFE, 0xDA, 0x7F, 0x9D, 0xBF, 0xEE, 0x5F, 0xEF, 0xAF,
            0xFF, 0x37, 0xF8, 0x1B, 0xFE, 0x8D, 0xFE, 0xC6, 0x7F, 0x93, 0xBF, 0xE9, 0xDF, 0xEC, 0x6F, 0xFE, 0xB7, 0xF8, 0x5B,
            0xFE, 0xAD, 0xFE, 0xD6, 0x7F, 0x9B, 0xBF, 0xED, 0xDF, 0xEE, 0x6F, 0xFF, 0x77, 0xF8, 0x3B, 0xFE, 0x9D, 0xFE, 0xCE,
            0x7F, 0x97, 0xBF, 0xEB, 0xDF, 0xED, 0xEF, 0xFE, 0xF7, 0xF8, 0x7B, 0xFE, 0xBD, 0xFE, 0xDE, 0x7F, 0x9F, 0xBF, 0xEF,
            0xDF, 0xEF, 0xEF, 0xFF, 0x0F, 0xF8, 0x07, 0xFE, 0x83, 0xFE, 0xC1, 0xFF, 0x90, 0x7F, 0xE8, 0x3F, 0xEC, 0x1F, 0xFE,
            0x8F, 0xF8, 0x47, 0xFE, 0xA3, 0xFE, 0xD1, 0xFF, 0x98, 0x7F, 0xEC, 0x3F, 0xEE, 0x1F, 0xFF, 0x4F, 0xF8, 0x27, 0xFE,
            0x93, 0xFE, 0xC9, 0xFF, 0x94, 0x7F, 0xEA, 0x3F, 0xED, 0x9F, 0xFE, 0xCF, 0xF8, 0x67, 0xFE, 0xB3, 0xFE, 0xD9, 0xFF,
            0x9C, 0x7F, 0xEE, 0x3F, 0xEF, 0x9F, 0xFF, 0x2F, 0xF8, 0x17, 0xFE, 0x8B, 0xFE, 0xC5, 0xFF, 0x92, 0x7F, 0xE9, 0xBF,
            0xEC, 0x5F, 0xFE, 0xAF, 0xF8, 0x57, 0xFE, 0xAB, 0xFE, 0xD5, 0xFF, 0x9A, 0x7F, 0xED, 0xBF, 0xEE, 0x5F, 0xFF, 0x6F,
            0xF8, 0x37, 0xFE, 0x9B, 0xFE, 0xCD, 0xFF, 0x96, 0x7F, 0xEB, 0xBF, 0xED, 0xDF, 0xFE, 0xEF, 0xF8, 0x77, 0xFE, 0xBB,
            0xFE, 0xDD, 0xFF, 0x9E, 0x7F, 0xEF, 0xBF, 0xEF, 0xDF, 0xFF, 0x1F, 0xF8, 0x0F, 0xFE, 0x87, 0xFE, 0xC3, 0xFF, 0x91,
            0xFF, 0xE8, 0x7F, 0xEC, 0x3F, 0xFE, 0x9F, 0xF8, 0x4F, 0xFE, 0xA7, 0xFE, 0xD3, 0xFF, 0x99, 0xFF, 0xEC, 0x7F, 0xEE,
            0x3F, 0xFF, 0x5F, 0xF8, 0x2F, 0xFE, 0x97, 0xFE, 0xCB, 0xFF, 0x95, 0xFF, 0xEA, 0x7F, 0xED, 0xBF, 0xFE, 0xDF, 0xF8,
            0x6F, 0xFE, 0xB7, 0xFE, 0xDB, 0xFF, 0x9D, 0xFF, 0xEE, 0x7F, 0xEF, 0xBF, 0xFF, 0x3F, 0xF8, 0x1F, 0xFE, 0x8F, 0xFE,
            0xC7, 0xFF, 0x93, 0xFF, 0xE9, 0xFF, 0xEC, 0x7F, 0xFE, 0xBF, 0xF8, 0x5F, 0xFE, 0xAF, 0xFE, 0xD7, 0xFF, 0x9B, 0xFF,
            0xED, 0xFF, 0xEE, 0x7F, 0xFF, 0x7F, 0xF8, 0x3F, 0xFE, 0x9F, 0xFE, 0xCF, 0xFF, 0x97, 0xFF, 0xEB, 0xFF, 0xED, 0xFF,
            0xFE, 0xFF, 0xF8, 0x7F, 0xFE, 0xBF, 0xFE, 0xDF, 0xFF, 0x9F, 0xFF, 0xEF, 0xFF, 0xEF, 0xFF, 0x0F};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Sparse tree")
    {
        // sparse in the sense that only some values (even values) have codes (144 values requires 8 bits per code)
        const uint8_t codeLengths[288] = {
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0,
            8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0, 8, 0};

        const uint16_t output[] = {0,   2,   4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,
                                   36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,  60,  62,  64,  66,  68,  70,
                                   72,  74,  76,  78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98,  100, 102, 104, 106,
                                   108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 142,
                                   144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178,
                                   180, 182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214,
                                   216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 238, 240, 242, 244, 246, 248, 250,
                                   252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272, 274, 276, 278, 280, 282, 284, 286};
        const uint8_t input[] = {0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
                                 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
                                 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
                                 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
                                 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
                                 0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
                                 0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
                                 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
                                 0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Tall tree")
    {
        // Same as the balanced tree, only with a max height (15 bits per code)
        const uint8_t codeLengths[288] = {
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {
            0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
            23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,
            46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,
            69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
            92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
            115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
            138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
            161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
            184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
            207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
            230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252,
            253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275,
            276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287};
        const uint8_t input[] = {
            0x00, 0x00, 0x00, 0x20, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x01, 0x80, 0x02, 0xC0, 0x00, 0xE0, 0x00, 0x08, 0x00, 0x24,
            0x00, 0x0A, 0x00, 0x0D, 0x80, 0x01, 0xC0, 0x02, 0xE0, 0x00, 0xF0, 0x00, 0x04, 0x00, 0x22, 0x00, 0x09, 0x80, 0x0C,
            0x40, 0x01, 0xA0, 0x02, 0xD0, 0x00, 0xE8, 0x00, 0x0C, 0x00, 0x26, 0x00, 0x0B, 0x80, 0x0D, 0xC0, 0x01, 0xE0, 0x02,
            0xF0, 0x00, 0xF8, 0x00, 0x02, 0x00, 0x21, 0x80, 0x08, 0x40, 0x0C, 0x20, 0x01, 0x90, 0x02, 0xC8, 0x00, 0xE4, 0x00,
            0x0A, 0x00, 0x25, 0x80, 0x0A, 0x40, 0x0D, 0xA0, 0x01, 0xD0, 0x02, 0xE8, 0x00, 0xF4, 0x00, 0x06, 0x00, 0x23, 0x80,
            0x09, 0xC0, 0x0C, 0x60, 0x01, 0xB0, 0x02, 0xD8, 0x00, 0xEC, 0x00, 0x0E, 0x00, 0x27, 0x80, 0x0B, 0xC0, 0x0D, 0xE0,
            0x01, 0xF0, 0x02, 0xF8, 0x00, 0xFC, 0x00, 0x01, 0x80, 0x20, 0x40, 0x08, 0x20, 0x0C, 0x10, 0x01, 0x88, 0x02, 0xC4,
            0x00, 0xE2, 0x00, 0x09, 0x80, 0x24, 0x40, 0x0A, 0x20, 0x0D, 0x90, 0x01, 0xC8, 0x02, 0xE4, 0x00, 0xF2, 0x00, 0x05,
            0x80, 0x22, 0x40, 0x09, 0xA0, 0x0C, 0x50, 0x01, 0xA8, 0x02, 0xD4, 0x00, 0xEA, 0x00, 0x0D, 0x80, 0x26, 0x40, 0x0B,
            0xA0, 0x0D, 0xD0, 0x01, 0xE8, 0x02, 0xF4, 0x00, 0xFA, 0x00, 0x03, 0x80, 0x21, 0xC0, 0x08, 0x60, 0x0C, 0x30, 0x01,
            0x98, 0x02, 0xCC, 0x00, 0xE6, 0x00, 0x0B, 0x80, 0x25, 0xC0, 0x0A, 0x60, 0x0D, 0xB0, 0x01, 0xD8, 0x02, 0xEC, 0x00,
            0xF6, 0x00, 0x07, 0x80, 0x23, 0xC0, 0x09, 0xE0, 0x0C, 0x70, 0x01, 0xB8, 0x02, 0xDC, 0x00, 0xEE, 0x00, 0x0F, 0x80,
            0x27, 0xC0, 0x0B, 0xE0, 0x0D, 0xF0, 0x01, 0xF8, 0x02, 0xFC, 0x00, 0xFE, 0x80, 0x00, 0x40, 0x20, 0x20, 0x08, 0x10,
            0x0C, 0x08, 0x01, 0x84, 0x02, 0xC2, 0x00, 0xE1, 0x80, 0x08, 0x40, 0x24, 0x20, 0x0A, 0x10, 0x0D, 0x88, 0x01, 0xC4,
            0x02, 0xE2, 0x00, 0xF1, 0x80, 0x04, 0x40, 0x22, 0x20, 0x09, 0x90, 0x0C, 0x48, 0x01, 0xA4, 0x02, 0xD2, 0x00, 0xE9,
            0x80, 0x0C, 0x40, 0x26, 0x20, 0x0B, 0x90, 0x0D, 0xC8, 0x01, 0xE4, 0x02, 0xF2, 0x00, 0xF9, 0x80, 0x02, 0x40, 0x21,
            0xA0, 0x08, 0x50, 0x0C, 0x28, 0x01, 0x94, 0x02, 0xCA, 0x00, 0xE5, 0x80, 0x0A, 0x40, 0x25, 0xA0, 0x0A, 0x50, 0x0D,
            0xA8, 0x01, 0xD4, 0x02, 0xEA, 0x00, 0xF5, 0x80, 0x06, 0x40, 0x23, 0xA0, 0x09, 0xD0, 0x0C, 0x68, 0x01, 0xB4, 0x02,
            0xDA, 0x00, 0xED, 0x80, 0x0E, 0x40, 0x27, 0xA0, 0x0B, 0xD0, 0x0D, 0xE8, 0x01, 0xF4, 0x02, 0xFA, 0x00, 0xFD, 0x80,
            0x01, 0xC0, 0x20, 0x60, 0x08, 0x30, 0x0C, 0x18, 0x01, 0x8C, 0x02, 0xC6, 0x00, 0xE3, 0x80, 0x09, 0xC0, 0x24, 0x60,
            0x0A, 0x30, 0x0D, 0x98, 0x01, 0xCC, 0x02, 0xE6, 0x00, 0xF3, 0x80, 0x05, 0xC0, 0x22, 0x60, 0x09, 0xB0, 0x0C, 0x58,
            0x01, 0xAC, 0x02, 0xD6, 0x00, 0xEB, 0x80, 0x0D, 0xC0, 0x26, 0x60, 0x0B, 0xB0, 0x0D, 0xD8, 0x01, 0xEC, 0x02, 0xF6,
            0x00, 0xFB, 0x80, 0x03, 0xC0, 0x21, 0xE0, 0x08, 0x70, 0x0C, 0x38, 0x01, 0x9C, 0x02, 0xCE, 0x00, 0xE7, 0x80, 0x0B,
            0xC0, 0x25, 0xE0, 0x0A, 0x70, 0x0D, 0xB8, 0x01, 0xDC, 0x02, 0xEE, 0x00, 0xF7, 0x80, 0x07, 0xC0, 0x23, 0xE0, 0x09,
            0xF0, 0x0C, 0x78, 0x01, 0xBC, 0x02, 0xDE, 0x00, 0xEF, 0x80, 0x0F, 0xC0, 0x27, 0xE0, 0x0B, 0xF0, 0x0D, 0xF8, 0x01,
            0xFC, 0x02, 0xFE, 0x00, 0xFF, 0x40, 0x00, 0x20, 0x20, 0x10, 0x08, 0x08, 0x0C, 0x04, 0x01, 0x82, 0x02, 0xC1, 0x80,
            0xE0, 0x40, 0x08, 0x20, 0x24, 0x10, 0x0A, 0x08, 0x0D, 0x84, 0x01, 0xC2, 0x02, 0xE1, 0x80, 0xF0, 0x40, 0x04, 0x20,
            0x22, 0x10, 0x09, 0x88, 0x0C, 0x44, 0x01, 0xA2, 0x02, 0xD1, 0x80, 0xE8, 0x40, 0x0C, 0x20, 0x26, 0x10, 0x0B, 0x88,
            0x0D, 0xC4, 0x01, 0xE2, 0x02, 0xF1, 0x80, 0xF8};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Short tree")
    {
        // Like the sparse tree, but REALLY sparse; only two entries with a single bit code size
        const uint8_t codeLengths[288] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};

        const uint16_t output[] = {282, 282, 38,  38,  282, 38,  38,  282, 38,  282, 38, 38,  282, 38,  38,  38,
                                   38,  282, 282, 282, 38,  282, 282, 38,  38,  38,  38, 38,  38,  38,  38,  282,
                                   282, 282, 282, 282, 38,  38,  282, 38,  282, 38,  38, 282, 282, 282, 282, 38,
                                   282, 38,  282, 282, 38,  282, 282, 38,  282, 38,  38, 38,  282, 282, 282, 282};
        const uint8_t input[] = {0x93, 0x12, 0x6E, 0x80, 0x4F, 0x79, 0x6D, 0xF1};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }

    SECTION("Memory Usage")
    {
        // Constructs a tree with the goal that said tree consumes the most memory possible. Each sub-tree represents 5
        // bits, which gives a max of 32 leaf nodes per sub-tree. This means that we need at least 9 sub-trees to reach
        // 288 total leaf nodes (288 / 32 = 9). In order to test maximum memory usage, we need a single sub-tree with
        // 31 leaf nodes, 8 sub-trees with 32 leaf nodes, and a final sub-tree with a single leaf node at max height.
        const uint8_t codeLengths[288] = {
            14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};

        const uint16_t output[] = {
            0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,
            23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,
            46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,
            69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
            92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
            115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137,
            138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
            161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183,
            184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206,
            207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229,
            230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252,
            253, 254, 255, 256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275,
            276, 277, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287};
        const uint8_t input[] = {
            0x00, 0x00, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x01, 0x80, 0x02, 0xC0, 0x00, 0xE0, 0x00, 0x08, 0x00, 0x24, 0x00, 0x0A,
            0x00, 0x0D, 0x80, 0x01, 0xC0, 0x02, 0xE0, 0x00, 0xF0, 0x00, 0x04, 0x00, 0x22, 0x00, 0x09, 0x80, 0x0C, 0x40, 0x01,
            0xA0, 0x02, 0xD0, 0x00, 0xE8, 0x00, 0x0C, 0x00, 0x26, 0x00, 0x0B, 0x80, 0x0D, 0xC0, 0x01, 0xE0, 0x02, 0xF0, 0x00,
            0xF8, 0x00, 0x02, 0x00, 0x21, 0x80, 0x08, 0x40, 0x0C, 0x20, 0x01, 0x90, 0x02, 0xC8, 0x00, 0xE4, 0x00, 0x0A, 0x00,
            0x25, 0x80, 0x0A, 0x40, 0x0D, 0xA0, 0x01, 0xD0, 0x02, 0xE8, 0x00, 0xF4, 0x00, 0x06, 0x00, 0x23, 0x80, 0x09, 0xC0,
            0x0C, 0x60, 0x01, 0xB0, 0x02, 0xD8, 0x00, 0xEC, 0x00, 0x0E, 0x00, 0x27, 0x80, 0x0B, 0xC0, 0x0D, 0xE0, 0x01, 0xF0,
            0x02, 0xF8, 0x00, 0xFC, 0x00, 0x01, 0x80, 0x20, 0x40, 0x08, 0x20, 0x0C, 0x10, 0x01, 0x88, 0x02, 0xC4, 0x00, 0xE2,
            0x00, 0x09, 0x80, 0x24, 0x40, 0x0A, 0x20, 0x0D, 0x90, 0x01, 0xC8, 0x02, 0xE4, 0x00, 0xF2, 0x00, 0x05, 0x80, 0x22,
            0x40, 0x09, 0xA0, 0x0C, 0x50, 0x01, 0xA8, 0x02, 0xD4, 0x00, 0xEA, 0x00, 0x0D, 0x80, 0x26, 0x40, 0x0B, 0xA0, 0x0D,
            0xD0, 0x01, 0xE8, 0x02, 0xF4, 0x00, 0xFA, 0x00, 0x03, 0x80, 0x21, 0xC0, 0x08, 0x60, 0x0C, 0x30, 0x01, 0x98, 0x02,
            0xCC, 0x00, 0xE6, 0x00, 0x0B, 0x80, 0x25, 0xC0, 0x0A, 0x60, 0x0D, 0xB0, 0x01, 0xD8, 0x02, 0xEC, 0x00, 0xF6, 0x00,
            0x07, 0x80, 0x23, 0xC0, 0x09, 0xE0, 0x0C, 0x70, 0x01, 0xB8, 0x02, 0xDC, 0x00, 0xEE, 0x00, 0x0F, 0x80, 0x27, 0xC0,
            0x0B, 0xE0, 0x0D, 0xF0, 0x01, 0xF8, 0x02, 0xFC, 0x00, 0xFE, 0x80, 0x00, 0x40, 0x20, 0x20, 0x08, 0x10, 0x0C, 0x08,
            0x01, 0x84, 0x02, 0xC2, 0x00, 0xE1, 0x80, 0x08, 0x40, 0x24, 0x20, 0x0A, 0x10, 0x0D, 0x88, 0x01, 0xC4, 0x02, 0xE2,
            0x00, 0xF1, 0x80, 0x04, 0x40, 0x22, 0x20, 0x09, 0x90, 0x0C, 0x48, 0x01, 0xA4, 0x02, 0xD2, 0x00, 0xE9, 0x80, 0x0C,
            0x40, 0x26, 0x20, 0x0B, 0x90, 0x0D, 0xC8, 0x01, 0xE4, 0x02, 0xF2, 0x00, 0xF9, 0x80, 0x02, 0x40, 0x21, 0xA0, 0x08,
            0x50, 0x0C, 0x28, 0x01, 0x94, 0x02, 0xCA, 0x00, 0xE5, 0x80, 0x0A, 0x40, 0x25, 0xA0, 0x0A, 0x50, 0x0D, 0xA8, 0x01,
            0xD4, 0x02, 0xEA, 0x00, 0xF5, 0x80, 0x06, 0x40, 0x23, 0xA0, 0x09, 0xD0, 0x0C, 0x68, 0x01, 0xB4, 0x02, 0xDA, 0x00,
            0xED, 0x80, 0x0E, 0x40, 0x27, 0xA0, 0x0B, 0xD0, 0x0D, 0xE8, 0x01, 0xF4, 0x02, 0xFA, 0x00, 0xFD, 0x80, 0x01, 0xC0,
            0x20, 0x60, 0x08, 0x30, 0x0C, 0x18, 0x01, 0x8C, 0x02, 0xC6, 0x00, 0xE3, 0x80, 0x09, 0xC0, 0x24, 0x60, 0x0A, 0x30,
            0x0D, 0x98, 0x01, 0xCC, 0x02, 0xE6, 0x00, 0xF3, 0x80, 0x05, 0xC0, 0x22, 0x60, 0x09, 0xB0, 0x0C, 0x58, 0x01, 0xAC,
            0x02, 0xD6, 0x00, 0xEB, 0x80, 0x0D, 0xC0, 0x26, 0x60, 0x0B, 0xB0, 0x0D, 0xD8, 0x01, 0xEC, 0x02, 0xF6, 0x00, 0xFB,
            0x80, 0x03, 0xC0, 0x21, 0xE0, 0x08, 0x70, 0x0C, 0x38, 0x01, 0x9C, 0x02, 0xCE, 0x00, 0xE7, 0x80, 0x0B, 0xC0, 0x25,
            0xE0, 0x0A, 0x70, 0x0D, 0xB8, 0x01, 0xDC, 0x02, 0xEE, 0x00, 0xF7, 0x80, 0x07, 0xC0, 0x23, 0xE0, 0x09, 0xF0, 0x0C,
            0x78, 0x01, 0xBC, 0x02, 0xDE, 0x00, 0xEF, 0x80, 0x0F, 0xC0, 0x27, 0xE0, 0x0B, 0xF0, 0x0D, 0xF8, 0x01, 0xFC, 0x02,
            0xFE, 0x00, 0xFF, 0x40, 0x00, 0x20, 0x20, 0x10, 0x08, 0x08, 0x0C, 0x04, 0x01, 0x82, 0x02, 0xC1, 0x80, 0xE0, 0x40,
            0x08, 0x20, 0x24, 0x10, 0x0A, 0x08, 0x0D, 0x84, 0x01, 0xC2, 0x02, 0xE1, 0x80, 0xF0, 0x40, 0x04, 0x20, 0x22, 0x10,
            0x09, 0x88, 0x0C, 0x44, 0x01, 0xA2, 0x02, 0xD1, 0x80, 0xE8, 0x40, 0x0C, 0x20, 0x26, 0x10, 0x0B, 0x88, 0x0D, 0xC4,
            0x01, 0xE2, 0x02, 0xF1, 0x80, 0xF8, 0x40, 0x02};
        DoHuffmanTreeTest(codeLengths, std::size(codeLengths), input, std::size(input), output, std::size(output));
    }
}

TEST_CASE("HuffmanTreeFailureTests", "[huffman_tree]")
{
    // Various scenarios where operation(s) on the Huffman Tree should fail
    auto doFailingTest = [](size_t codeLengthsSize, auto&& callback) {
        inflatelib_stream stream = {};
        REQUIRE(inflatelib_init(&stream) == INFLATELIB_OK);

        huffman_tree tree = {};
        REQUIRE(huffman_tree_init(&tree, &stream, codeLengthsSize) == INFLATELIB_OK);
        callback(stream, tree);
        huffman_tree_destroy(&tree, &stream);
        inflatelib_destroy(&stream);
    };

    SECTION("Invalid Lengths")
    {
        // A set of tests that should fail Huffman Tree creation due to an invalid tree (too many code lengths)
        SECTION("1-bit Height")
        {
            // Max of 2 symbols with a 1-bit height; 3 is an error
            const uint8_t lens[19] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
            doFailingTest(std::size(lens), [&](inflatelib_stream& stream, huffman_tree& tree) {
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_ERROR_DATA);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Too many symbols with code length 1. 3 symbols starting at 0x0 exceeds the specified number of bits"sv);
            });
        }

        SECTION("2-bit Height")
        {
            // Max of 4 symbols with a 2-bit height; 5 is an error
            const uint8_t lens[19] = {2, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0};
            doFailingTest(std::size(lens), [&](inflatelib_stream& stream, huffman_tree& tree) {
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_ERROR_DATA);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Too many symbols with code length 2. 5 symbols starting at 0x0 exceeds the specified number of bits"sv);
            });
        }

        SECTION("3-bit Height")
        {
            // Max of 8 symbols with a 3-bit height. This test is slightly different than the ones above. Instead of
            // having 9 symbols with a length of 3, we'll have 8 and then a single with a length of 4
            const uint8_t lens[19] = {0, 4, 0, 3, 0, 0, 3, 0, 3, 3, 0, 0, 3, 0, 0, 0, 3, 3, 3};
            doFailingTest(std::size(lens), [&](inflatelib_stream& stream, huffman_tree& tree) {
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_ERROR_DATA);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Too many symbols with code length 4. 1 symbols starting at 0x10 exceeds the specified number of bits"sv);
            });
        }

        SECTION("Unbalanced")
        {
            // Similar to the 3-bit height test. Construct a right leaning tree, but with too many nodes at the "bottom"
            const uint8_t lens[19] = {8, 4, 12, 1, 11, 10, 15, 6, 7, 13, 3, 15, 2, 0, 9, 0, 5, 15, 14};
            doFailingTest(std::size(lens), [&](inflatelib_stream& stream, huffman_tree& tree) {
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_ERROR_DATA);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Too many symbols with code length 15. 3 symbols starting at 0x7FFE exceeds the specified number of bits"sv);
            });
        }
    }

    SECTION("Invalid Input")
    {
        // A set of tests that should fail because the input corresponds to unassigned nodes
        SECTION("First Symbol")
        {
            doFailingTest(19, [](inflatelib_stream& stream, huffman_tree& tree) {
                const uint8_t lens[19] = {6, 0, 0, 0, 5, 0, 0, 0, 5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0};
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_OK);

                // The last element takes path 000110
                const uint8_t input = 0x38; // 000111 (aka 111000)
                uint16_t output;
                bitstream_set_data(&stream.internal->bitstream, &input, 1);
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) < 0);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Input bit sequence 0x38 is not a valid Huffman code for the encoded table"sv);
            });

            doFailingTest(19, [](inflatelib_stream& stream, huffman_tree& tree) {
                const uint8_t lens[19] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_OK);

                // 19 values at max height acccount for paths 0000000 to 0010010
                const uint8_t input = 0x64; // 0010011 aka 110 0100
                uint16_t output;
                bitstream_set_data(&stream.internal->bitstream, &input, 1);
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) < 0);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Input bit sequence 0x64 is not a valid Huffman code for the encoded table"sv);
            });

            doFailingTest(32, [](inflatelib_stream& stream, huffman_tree& tree) {
                const uint8_t lens[32] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                                          15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_OK);

                // 32 values at max height acccount for paths 000000000000000 to 000000000011111
                const uint8_t input[] = {0x00, 0x02}; // 000000000100000 (aka 0000010 00000000)
                uint16_t output;
                bitstream_set_data(&stream.internal->bitstream, input, std::size(input));
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) < 0);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Input bit sequence 0x200 is not a valid Huffman code for the encoded table"sv);
            });
        }

        SECTION("Nth Symbol")
        {
            doFailingTest(19, [](inflatelib_stream& stream, huffman_tree& tree) {
                const uint8_t lens[19] = {6, 0, 0, 0, 5, 0, 0, 0, 5, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0};
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_OK);

                // The last element takes path 000110 (aka 011000)
                const uint8_t input = 0x90; // 00001 001 (aka 100 10000)
                uint16_t output;
                bitstream_set_data(&stream.internal->bitstream, &input, 1);
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) > 0);
                REQUIRE(output == 8);

                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) < 0);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Input bit sequence 0x4 is not a valid Huffman code for the encoded table"sv);
            });

            doFailingTest(32, [](inflatelib_stream& stream, huffman_tree& tree) {
                const uint8_t lens[32] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 14, 15, 15, 15, 15,
                                          15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};
                REQUIRE(huffman_tree_reset(&tree, &stream, lens, std::size(lens)) == INFLATELIB_OK);

                // Valid paths are 0...0 to 000000000100000. The only 14-bit long element is for the symbol at index 11
                // The following corresponds to the input:
                //      00000000000000 000000000100000 000000000001010 000000000010101 000000000100010
                const uint8_t input[] = {0x00, 0x00, 0x80, 0x00, 0x00, 0x05, 0x40, 0x05, 0x10, 0x01};
                bitstream_set_data(&stream.internal->bitstream, input, std::size(input));

                uint16_t output;
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) > 0);
                REQUIRE(output == 11);

                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) > 0);
                REQUIRE(output == 31);

                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) > 0);
                REQUIRE(output == 8);

                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) > 0);
                REQUIRE(output == 20);

                // Invalid input is 010001000000000
                REQUIRE(huffman_tree_lookup(&tree, &stream, &output) < 0);
                REQUIRE(errno == EINVAL);
                REQUIRE(stream.error_msg == "Input bit sequence 0x2200 is not a valid Huffman code for the encoded table"sv);
            });
        }
    }
}
