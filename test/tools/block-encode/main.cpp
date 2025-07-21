/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <cassert>
#include <charconv>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <queue>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "max_heap.h"

using namespace std::literals;

#define IGNORED_CHARACTERS " \t,;.#"

struct sized_offset_encoding_data
{
    std::uint32_t base_offset;
    std::uint8_t extra_bits;
    std::uint32_t max_offset;
    std::uint16_t max_extra_data;

    constexpr sized_offset_encoding_data(std::uint32_t baseOffset, std::uint8_t extraBits) noexcept :
        base_offset(baseOffset), extra_bits(extraBits)
    {
        max_extra_data = static_cast<std::uint16_t>((1 << extra_bits) - 1);
        max_offset = base_offset + max_extra_data;
    }
};

// Index by subtracting 257
static constexpr const sized_offset_encoding_data deflate_length_encoding_data[] = {
    {3, 0},   // 257
    {4, 0},   // 258
    {5, 0},   // 259
    {6, 0},   // 260
    {7, 0},   // 261
    {8, 0},   // 262
    {9, 0},   // 263
    {10, 0},  // 264
    {11, 1},  // 265
    {13, 1},  // 266
    {15, 1},  // 267
    {17, 1},  // 268
    {19, 2},  // 269
    {23, 2},  // 270
    {27, 2},  // 271
    {31, 2},  // 272
    {35, 3},  // 273
    {43, 3},  // 274
    {51, 3},  // 275
    {59, 3},  // 276
    {67, 4},  // 277
    {83, 4},  // 278
    {99, 4},  // 279
    {115, 4}, // 280
    {131, 5}, // 281
    {163, 5}, // 282
    {195, 5}, // 283
    {227, 5}, // 284
    {258, 0}, // 285
};

static constexpr const sized_offset_encoding_data deflate64_length_encoding_data[] = {
    {3, 0},   // 257
    {4, 0},   // 258
    {5, 0},   // 259
    {6, 0},   // 260
    {7, 0},   // 261
    {8, 0},   // 262
    {9, 0},   // 263
    {10, 0},  // 264
    {11, 1},  // 265
    {13, 1},  // 266
    {15, 1},  // 267
    {17, 1},  // 268
    {19, 2},  // 269
    {23, 2},  // 270
    {27, 2},  // 271
    {31, 2},  // 272
    {35, 3},  // 273
    {43, 3},  // 274
    {51, 3},  // 275
    {59, 3},  // 276
    {67, 4},  // 277
    {83, 4},  // 278
    {99, 4},  // 279
    {115, 4}, // 280
    {131, 5}, // 281
    {163, 5}, // 282
    {195, 5}, // 283
    {227, 5}, // 284
    {3, 16},  // 285
};

static constexpr const sized_offset_encoding_data distance_encoding_data[] = {
    {1, 0},      // 0
    {2, 0},      // 1
    {3, 0},      // 2
    {4, 0},      // 3
    {5, 1},      // 4
    {7, 1},      // 5
    {9, 2},      // 6
    {13, 2},     // 7
    {17, 3},     // 8
    {25, 3},     // 9
    {33, 4},     // 10
    {49, 4},     // 11
    {65, 5},     // 12
    {97, 5},     // 13
    {129, 6},    // 14
    {193, 6},    // 15
    {257, 7},    // 16
    {385, 7},    // 17
    {513, 8},    // 18
    {769, 8},    // 19
    {1025, 9},   // 20
    {1537, 9},   // 21
    {2049, 10},  // 22
    {3073, 10},  // 23
    {4097, 11},  // 24
    {6145, 11},  // 25
    {8193, 12},  // 26
    {12289, 12}, // 27
    {16385, 13}, // 28
    {24577, 13}, // 29
    {32769, 14}, // 30
    {49153, 14}, // 31
};

struct encoding_data
{
    std::span<const sized_offset_encoding_data> lengths;
    std::span<const sized_offset_encoding_data> distances;
};

static constexpr const encoding_data deflate_encoding_data = {deflate_length_encoding_data, {distance_encoding_data, 30}};
static constexpr const encoding_data deflate64_encoding_data = {deflate64_length_encoding_data, distance_encoding_data};

template <typename T>
static bool read_number(const std::string& str, std::string::size_type& pos, T& result)
{
    auto startPos = str.find_first_not_of(IGNORED_CHARACTERS, pos);
    if (startPos == std::string::npos)
    {
        std::println("ERROR: Numeric input missing from {}", str.substr(pos));
        return false;
    }

    auto endPos = startPos;
    while ((endPos < str.size()) && std::isalnum(str[endPos]))
    {
        ++endPos;
    }

    auto begin = str.data() + startPos;
    auto end = str.data() + endPos;
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if ((ec != std::errc{}) || (ptr != end))
    {
        std::println("ERROR: '{}' is not a valid {}-bit integer", str.substr(startPos, endPos - startPos), 8 * sizeof(T));
        return false;
    }

    pos = endPos;

    return true;
}

enum class symbol_type
{
    literal_length,
    distance,
};

struct output_symbol
{
    symbol_type type;
    std::uint16_t symbol;
    std::uint16_t extra_data;

    static output_symbol make_literal(std::uint16_t symbol) noexcept
    {
        return {symbol_type::literal_length, symbol, 0};
    }

    static output_symbol make_length(std::uint16_t symbol, std::uint16_t extraData) noexcept
    {
        return {symbol_type::literal_length, symbol, extraData};
    }

    static output_symbol make_distance(std::uint16_t symbol, std::uint16_t extraData) noexcept
    {
        return {symbol_type::distance, symbol, extraData};
    }
};

static bool read_input_as_symbols(std::istream& stream, const encoding_data& encoding, std::vector<output_symbol>& output)
{
    std::string line;
    while (std::getline(stream, line) && !line.empty())
    {
        auto index = line.find_first_not_of(IGNORED_CHARACTERS);
        if (index == std::string::npos)
        {
            break; // Treat the same as an empty line
        }

        switch (line[index])
        {
        case '\'':
            if ((line.size() < (index + 3)) || (line[index + 2] != '\''))
            {
                std::println("ERROR: Invalid/incomplete character: '{}'", line.substr(index));
                return false;
            }

            output.push_back(output_symbol::make_literal(static_cast<std::uint16_t>(line[index + 1])));
            index += 3;
            break;

        case '"':
            for (auto pos = index + 1;;)
            {
                auto nextPos = line.find_first_of("\"\\", pos);
                if (nextPos == std::string::npos)
                {
                    std::println("ERROR: Unterminated string: '{}'", line.substr(index));
                    return false;
                }

                // All characters up until 'nextPos' need to get added to the output
                for (auto i = pos; i < nextPos; ++i)
                {
                    output.push_back(output_symbol::make_literal(static_cast<std::uint16_t>(line[i])));
                }

                if (line[nextPos] == '\\')
                {
                    // Escaped character
                    if (nextPos == (line.size() - 1))
                    {
                        std::println("ERROR: Unterminated string: '{}'", line.substr(index));
                        return false;
                    }

                    std::uint16_t symbol = 0;
                    switch (line[nextPos + 1])
                    {
                    case '0':
                        symbol = '\0';
                        break;
                    case 'n':
                        symbol = '\n';
                        break;
                    case 'r':
                        symbol = '\r';
                        break;
                    case 't':
                        symbol = '\t';
                        break;
                    case 'b':
                        symbol = '\b';
                        break;
                    case 'f':
                        symbol = '\f';
                        break;
                    case 'v':
                        symbol = '\v';
                        break;
                    case '\\':
                        symbol = '\\';
                        break;
                    case '"':
                        symbol = '"';
                        break;
                    default:
                        std::println("ERROR: Invalid escape sequence: '\\{}'", line[nextPos + 1]);
                        return false;
                    }

                    output.push_back(output_symbol::make_literal(symbol));
                    pos = nextPos + 2;
                }
                else
                {
                    // End of the string
                    assert(line[nextPos] == '"');
                    index = nextPos + 1;
                    break;
                }
            }
            break;

        case '(':
        {
            // Length/distance pair of the form "(<length>, <distance>)"
            std::uint32_t length = 0;
            auto pos = index + 1;
            if (!read_number(line, pos, length))
            {
                return 1;
            }

            // Next non-whitespace character should be a comma
            pos = line.find_first_not_of(" \t", pos);
            if ((pos == std::string::npos) || (line[pos] != ','))
            {
                std::println("ERROR: Expected ',' after length in '{}'", line.substr(index));
                return false;
            }

            // NOTE: We can just call the read function again, which will just skip the comma
            std::uint32_t distance = 0;
            if (!read_number(line, ++pos, distance))
            {
                return false;
            }

            pos = line.find_first_not_of(IGNORED_CHARACTERS, pos);
            if ((pos == std::string::npos) || (line[pos] != ')'))
            {
                std::println("ERROR: Expected ')' after distance in '{}'", line.substr(index));
                return false;
            }
            ++pos;

            // Finally, convert these values to actual data
            auto findInfo = [](std::span<const sized_offset_encoding_data> info, std::uint32_t value) -> std::size_t {
                for (std::size_t i = 0; i < info.size(); ++i)
                {
                    if ((value >= info[i].base_offset) && (value <= info[i].max_offset))
                    {
                        return i;
                    }
                }

                std::println("ERROR: Value '{}' does not match any valid range", value);
                std::println("NOTE: Expected a value between {} and {}", info.front().base_offset, info.back().max_offset);
                return info.size();
            };

            auto lengthIndex = findInfo(encoding.lengths, length);
            if (lengthIndex == encoding.lengths.size())
            {
                return false;
            }
            output.push_back(
                output_symbol::make_length(
                    static_cast<std::uint16_t>(257 + lengthIndex),
                    static_cast<std::uint16_t>(length - encoding.lengths[lengthIndex].base_offset)));

            auto distanceIndex = findInfo(encoding.distances, distance);
            if (distanceIndex == encoding.distances.size())
            {
                return false;
            }
            output.push_back(
                output_symbol::make_distance(
                    static_cast<std::uint16_t>(distanceIndex),
                    static_cast<std::uint16_t>(distance - encoding.distances[distanceIndex].base_offset)));

            index = pos;
            break;
        }

        default:
        {
            // Otherwise this is a number
            std::uint16_t symbol;
            if (!read_number(line, index, symbol))
            {
                return false;
            }

            if (symbol <= 256)
            {
                // Literal or end of block; either case there's no additional data
                output.push_back(output_symbol::make_literal(symbol));
                break;
            }
            else if (symbol > 285)
            {
                std::println("ERROR: Length symbol '{}' exceeds maximum of 285", symbol);
                return false;
            }

            std::uint16_t extraData = 0;
            auto& lengthInfo = encoding.lengths[symbol - 257];
            if (lengthInfo.extra_bits > 0)
            {
                index = line.find_first_not_of(IGNORED_CHARACTERS, index);
                if (index == std::string::npos)
                {
                    std::println("ERROR: Missing extra bits for symbol '{}'", symbol);
                    return false;
                }

                if (!read_number(line, index, extraData))
                {
                    return false;
                }

                if (extraData > lengthInfo.max_extra_data)
                {
                    std::println("ERROR: Extra data '{}' exceeds maximum of {}", extraData, lengthInfo.max_extra_data);
                    return false;
                }
            }

            output.push_back(output_symbol::make_length(symbol, extraData));

            // Now read the distance
            index = line.find_first_not_of(IGNORED_CHARACTERS, index);
            if (index == std::string::npos)
            {
                std::println("ERROR: Missing distance for symbol '{}'", symbol);
                return false;
            }

            if (!read_number(line, index, symbol))
            {
                return false;
            }

            if (symbol >= encoding.distances.size())
            {
                std::println("ERROR: Distance symbol '{}' exceeds maximum of {}", symbol, encoding.distances.size() - 1);
                return false;
            }

            extraData = 0;
            if (symbol >= 4)
            {
                index = line.find_first_not_of(IGNORED_CHARACTERS, index);
                if (index == std::string::npos)
                {
                    std::println("ERROR: Missing extra bits for distance '{}'", symbol);
                    return false;
                }

                if (!read_number(line, index, extraData))
                {
                    return false;
                }

                auto& distInfo = encoding.distances[symbol];
                if (extraData > distInfo.max_extra_data)
                {
                    std::println("ERROR: Extra data '{}' exceeds maximum of {}", extraData, distInfo.max_extra_data);
                    return false;
                }
            }

            output.push_back(output_symbol::make_distance(symbol, extraData));
            break;
        }
        }

        if (auto pos = line.find_first_not_of(IGNORED_CHARACTERS, index); pos != std::string::npos)
        {
            std::println("ERROR: Unexpected text: '{}'", line.substr(pos));
            return false;
        }
    }

    if (output.empty())
    {
        std::println("ERROR: No input data");
        return false;
    }

    return true;
}

template <typename T>
struct huffman_tree_node
{
    using symbol_type = T;

    T symbol = 0; // min symbol if not a leaf
    std::size_t count = 0;
    std::size_t max_depth = 0;
    std::unique_ptr<huffman_tree_node> left = nullptr;
    std::unique_ptr<huffman_tree_node> right = nullptr;
};

using literal_length_node = std::unique_ptr<huffman_tree_node<std::uint16_t>>;
using distance_node = std::unique_ptr<huffman_tree_node<std::uint8_t>>;

struct huffman_node_compare
{
    template <typename SymbolT>
    bool operator()(const std::unique_ptr<huffman_tree_node<SymbolT>>& lhs, const std::unique_ptr<huffman_tree_node<SymbolT>>& rhs) const
    {
        // NOTE: The STL is all sorts of backwards with the heap functions... They implement a *max* heap, but the
        // default argument is std::less (as opposed to a much more sane std::greater)... So the logic here is inverse
        // of what you expect. We want to return FALSE if we want lhs to have greater precedence in the heap and
        // therefore be consumed first
        if (lhs->count != rhs->count)
        {
            return lhs->count > rhs->count;
        }

        // We want to make trees as shallow as possible (precedence goes to the one with the shallower max depth)
        if (lhs->max_depth != rhs->max_depth)
        {
            return lhs->max_depth > rhs->max_depth;
        }

        // Otherwise it doesn't really matter which one we pick, however we want to be consistent with which one we
        // choose so that multiple runs yield the same result. In this case, we'll consume nodes with the greatest
        // minimum symbol first
        return lhs->symbol < rhs->symbol;
    }
};

template <typename SymbolT>
static std::unique_ptr<huffman_tree_node<SymbolT>> build_huffman_tree(
    max_heap<std::unique_ptr<huffman_tree_node<SymbolT>>, huffman_node_compare>& nodes)
{
    if (nodes.empty())
    {
        return nullptr;
    }

    std::unique_ptr<huffman_tree_node<SymbolT>> root;
    while (true)
    {
        root = nodes.pop();
        if (nodes.empty())
        {
            // Last node; we're done
            break;
        }

        auto next = nodes.pop();

        auto node = std::make_unique<huffman_tree_node<SymbolT>>();
        node->count = root->count + next->count;
        node->max_depth = std::max(root->max_depth, next->max_depth) + 1;
        node->symbol = std::min(root->symbol, next->symbol);
        node->left = std::move(next);
        node->right = std::move(root);
        nodes.push(std::move(node));
    }

    assert(root);
    if (!root->left)
    {
        // This is a special case where we only have one symbol to encode. We can't use zero bits - that would be silly -
        // so structure the tree such that we use just one bit. This is the only scenario where left can be non-null,
        // but right is null
        auto node = std::make_unique<huffman_tree_node<SymbolT>>();
        node->count = root->count;
        node->left = std::move(root);
        root.swap(node);
    }

    return root;
}

template <typename SymbolT, std::size_t N>
static std::unique_ptr<huffman_tree_node<SymbolT>> build_tree_from_counts(const std::size_t (&counts)[N])
{
    max_heap<std::unique_ptr<huffman_tree_node<SymbolT>>, huffman_node_compare> nodes;
    for (SymbolT i = 0; i < N; ++i)
    {
        if (counts[i])
        {
            auto node = std::make_unique<huffman_tree_node<SymbolT>>();
            node->symbol = i;
            node->count = counts[i];
            nodes.push(std::move(node));
        }
    }

    return build_huffman_tree(nodes);
}

static std::pair<std::unique_ptr<huffman_tree_node<std::uint16_t>>, std::unique_ptr<huffman_tree_node<std::uint8_t>>> build_trees_from_symbols(
    const std::vector<output_symbol>& output)
{
    // First build the counts of each symbol
    std::size_t literalLengthCounts[286] = {};
    std::size_t distanceCounts[32] = {};
    for (const auto& symbol : output)
    {
        switch (symbol.type)
        {
        case symbol_type::literal_length:
            ++literalLengthCounts[symbol.symbol];
            break;

        case symbol_type::distance:
            ++distanceCounts[symbol.symbol];
            break;
        }
    }

    return {build_tree_from_counts<std::uint16_t>(literalLengthCounts), build_tree_from_counts<std::uint8_t>(distanceCounts)};
}

template <typename SymbolT, std::size_t N>
static bool calculate_code_lens(
    const std::unique_ptr<huffman_tree_node<SymbolT>>& root, std::uint8_t depth, std::uint8_t (&lens)[N], SymbolT& maxSeen, SymbolT* symbolOrder = nullptr)
{
    if (root->left)
    {
        assert(root->right || (depth == 0));
        return calculate_code_lens(root->left, depth + 1, lens, maxSeen, symbolOrder) &&
               (!root->right || calculate_code_lens(root->right, depth + 1, lens, maxSeen, symbolOrder));
    }
    else
    {
        assert(!root->right);
        if (root->symbol >= std::size(lens))
        {
            std::println("ERROR: Symbol {} is out of range", root->symbol);
            return false;
        }
        else if (lens[root->symbol])
        {
            std::println("ERROR: Duplicate symbol {} in tree", root->symbol);
            return false;
        }

        auto symbolIndex = symbolOrder ? symbolOrder[root->symbol] : root->symbol;
        if (symbolIndex > maxSeen)
        {
            maxSeen = symbolIndex;
        }

        lens[root->symbol] = depth;
    }

    return true;
}

template <std::size_t N>
static bool calculate_codes(const std::uint8_t (&lens)[N], std::uint16_t (&codes)[N])
{
    std::uint16_t lengthCounts[16] = {};
    for (auto len : lens)
    {
        if (len >= 16)
        {
            std::println("ERROR: Huffman tree was calculated with a height greater than the maximum allowed");
            std::println("NOTE: This is a limitation of this implementation as it doesn't take max tree height into account");
            std::println("NOTE: when determining the structure of the tree. Different symbol counts will need to be used in");
            std::println("NOTE: order to generate a valid tree.");
            return false;
        }

        ++lengthCounts[len];
    }

    std::uint16_t bases[16] = {};
    std::uint16_t nextCode = 0;
    for (std::size_t i = 1; i < std::size(lengthCounts); ++i)
    {
        bases[i] = nextCode;
        nextCode = (nextCode + lengthCounts[i]) << 1;
    }

    for (std::size_t i = 0; i < N; ++i)
    {
        auto len = lens[i];
        if (len)
        {
            codes[i] = bases[len]++;
        }
    }

    return true;
}

void print_usage()
{
    std::println(R"^-^(
USAGE
    block-encode <deflate | deflate64> [input-path] [static]

DESCRIPTION
    Encodes the input data as a single block using the specified encoding. Note that this does NOT compress the data;
    it merely encodes the data as specified. Length/distance pairs can be specified either as a pair of the form
    '(<length>, <distance>)' or as explicit values of the form
    '<length-symbol> [length-extra-data] <distance-symbol> [distance-extra-data]'. In the latter case, you are
    responsible for knowing whether or not the specified symbol requires extra data as well as its range (0-2^N where N
    is the number of extra bits for the symbol). The output is text that can be used with the 'bin-write' executable to
    produce the binary output.

ARGUMENTS
    deflate | deflate64   Specifies how output data should be encoded.
    input-path            The path to the input file. If not provided, input will be read from stdin.
    static                Use the static tables for the encoding. If not provided, the input data will be used.
)^-^");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::println("ERROR: Too few arguments\n");
        return print_usage(), 1;
    }
    else if (argc > 4)
    {
        std::println("ERROR: Too many arguments\n");
        return print_usage(), 1;
    }

    const encoding_data* encoding = nullptr;
    int argIndex = 1;
    if (argv[argIndex] == "deflate"sv)
    {
        encoding = &deflate_encoding_data;
    }
    else if (argv[argIndex] == "deflate64"sv)
    {
        encoding = &deflate64_encoding_data;
    }
    else
    {
        std::println("ERROR: Unknown encoding type '{}'. Expected 'deflate' or 'deflate64'", argv[1]);
        return print_usage(), 1;
    }

    ++argIndex; // Required arg

    // Read in the output we are expected to generate
    std::vector<output_symbol> outputData;
    if ((argIndex < argc) && (argv[argIndex] != "static"sv))
    {
        // Input comes from a file
        std::ifstream file(argv[argIndex]);
        if (!file)
        {
            std::println("Failed to open file '{}'", argv[argIndex]);
            return 1;
        }

        if (!read_input_as_symbols(file, *encoding, outputData))
        {
            return 1;
        }

        ++argIndex;
    }
    else
    {
        // Input comes from stdin
        std::println("Enter output data in one of the following forms:");
        std::println("    <literal-length-symbol> <length-opt> <distance-symbol-opt> <distance-opt>");
        std::println("    '<char>'");
        std::println("    \"<string>\"");
        std::println("    (<length>, <distance>)");
        std::println("Enter an empty line to indicate completion:");
        if (!read_input_as_symbols(std::cin, *encoding, outputData))
        {
            return 1;
        }
    }

    // Construct the Huffman trees based off the command line input, if present. Otherwise use the input data
    std::uint8_t literalLengthCodeLens[288] = {};
    std::uint8_t distanceCodeLens[32] = {};
    std::uint16_t maxLiteralLength = 0;
    std::uint8_t maxDistance = 0;
    bool hasDistanceCodes = true;
    bool usingStaticTables = false;
    if (argIndex < argc)
    {
        std::string_view cmd = argv[argIndex];
        if (cmd == "static"sv)
        {
            usingStaticTables = true;

            // Use the static tables
            std::size_t i = 0;
            for (; i < 144; ++i)
            {
                literalLengthCodeLens[i] = 8;
            }
            for (; i < 256; ++i)
            {
                literalLengthCodeLens[i] = 9;
            }
            for (; i < 280; ++i)
            {
                literalLengthCodeLens[i] = 7;
            }
            for (; i < 288; ++i)
            {
                literalLengthCodeLens[i] = 8;
            }
            maxLiteralLength = 287;

            for (i = 0; i < 32; ++i)
            {
                distanceCodeLens[i] = 5;
            }
            maxDistance = 31;
        }
        else
        {
            // Input comes from a file
            // TODO
            std::println("ERROR: User-specified code lengths not yet implemented");
            return print_usage(), 1;
        }

        ++argIndex;
    }
    else
    {
        // Construct the Huffman trees for the literal/length and distance alphabets
        auto [literalLengthRoot, distanceRoot] = build_trees_from_symbols(outputData);

        // Figure out the number of bits per symbol & remember the max symbol for each alphabet since we'll need it later
        if (!calculate_code_lens(literalLengthRoot, 0, literalLengthCodeLens, maxLiteralLength))
        {
            return 1;
        }

        hasDistanceCodes = distanceRoot != nullptr;
        if (distanceRoot && !calculate_code_lens(distanceRoot, 0, distanceCodeLens, maxDistance))
        {
            return 1;
        }
    }

    // Figure out how we're going to write the code length codes table. We need to actually compress this, but it's
    // thankfully pretty easy and straightforward to do
    // First, write all the data we're going to write to a single array to make things easier
    // NOTE: Add one to each max because the range starts at zero
    auto hlit = static_cast<std::uint16_t>(std::max(maxLiteralLength + 1, 257));
    auto hdist = static_cast<std::uint8_t>(std::max(maxDistance + 1, 1));
    auto lensToEncodeCount = hlit + hdist;
    std::uint8_t lensToEncode[288 + 32] = {};
    for (std::size_t i = 0; i < hlit; ++i)
    {
        lensToEncode[i] = literalLengthCodeLens[i];
    }
    for (std::size_t i = 0; i < hdist; ++i)
    {
        lensToEncode[hlit + i] = distanceCodeLens[i];
    }

    // Now figure out the data we're going to write. Cache this so we don't have to re-calculate it later and run the
    // risk of doing something inconsistent and getting different data later
    std::pair<std::uint8_t, std::uint8_t> codeLens[288 + 32]; // .second is the value of extra bits, if appropriate
    std::size_t codeLenSize = 0;
    std::uint16_t codeLenCounts[19] = {};
    for (std::uint16_t i = 0; i < lensToEncodeCount;)
    {
        auto start = i;
        auto symbol = lensToEncode[start];
        while ((++i < lensToEncodeCount) && (lensToEncode[i] == symbol))
        {
            // This loop is just to advance 'i'
        }

        std::uint16_t count = i - start;
        if (symbol == 0)
        {
            // We can either:
            //      1.  Write literal zero(es)
            //      2.  Repeat zero 3-10 times
            //      3.  Repeat zero 11-138 times
            // NOTE: This isn't as easy as simple division. For example, if the repeat count is 140, blindly starting
            // out by writing a repeat count of 138 is actually sub-optimal since we would then have to write two
            // literal zeroes as the minimum repeat count is 3. It would also be inefficient to break up a repeat of 140
            // into two repeats of 70 since each of those requires 7 bits of length (as opposed to having a repeat of
            // 137 needing 7 extra bits followed by a repeat of 3 needing 3 extra bits). We solve this by being greedy
            // with our assignment for shorter repeats. This becomes additionally complicated with scenarios such as 139
            // since it's possible to use a repeat of 138 followed by a literal zero, the space efficiency of which is
            // dependent on both the code length of the literal zero as well as the code length of instead using a
            // repeat of 3, however we're currently in the process of determining both of those counts. We don't strive
            // to be _that_ perfect, so in such sitations we assume that a literal zero is the better option.
            std::uint16_t longRepeatCount = count / 138; // Might be one more
            auto remainder = static_cast<std::uint8_t>(count % 138);
            if (remainder == 0)
            {
                // Nothing to do; we'll properly handle this later
            }
            else if ((remainder == 1) || ((remainder == 2) && (longRepeatCount == 0)))
            {
                // See above, we'll just write a literal 0
                count -= remainder;
                codeLenCounts[0] += remainder;
                codeLens[codeLenSize++] = {};
                if (remainder == 2)
                {
                    // This will happen if count is originally 2. Nothing we can do except write it twice
                    assert((i - start) == 2);
                    codeLens[codeLenSize++] = {};
                }
            }
            else if (remainder <= 10)
            {
                // See comment above; be greedy and consume as much as we can
                auto toRepeat = (longRepeatCount != 0) ? static_cast<std::uint8_t>(10) : remainder;
                count -= toRepeat;
                ++codeLenCounts[17];
                codeLens[codeLenSize++] = {static_cast<std::uint8_t>(17), static_cast<std::uint8_t>(toRepeat - 3)};
            }
            else
            {
                // The remaining data goes into another long repeat
                ++longRepeatCount;
            }

            codeLenCounts[18] += longRepeatCount;
            while (longRepeatCount--)
            {
                auto toRepeat = std::min(count, static_cast<std::uint16_t>(138));
                assert(toRepeat >= 11);
                count -= toRepeat;
                codeLens[codeLenSize++] = {static_cast<std::uint8_t>(18), static_cast<std::uint8_t>(toRepeat - 11)};
            }
            assert(count == 0);
        }
        else
        {
            // For any non-zero symbols, we can either:
            //      1.  Write more literal codes
            //      2.  Repeat the code 3-6 times
            // See the long comment above about why this isn't trivial to do. Unlike the above, we only have one option
            // for specifying repeats, so we can't do the "trick" above of being greedy. E.g. if 'count' was originally
            // 9, we need to write one literal plus another 8 lengths. Starting with a repeat of 6 means we need to
            // write two more literals, which can't fit in a repeat. Instead, it's better to do two repeats of 4 and 4
            // or 5 and 3.
            std::uint16_t literalCount = (count > 3) ? 1 : count;
            count -= literalCount;
            codeLenCounts[symbol] += literalCount;
            while (literalCount--)
            {
                codeLens[codeLenSize++] = {symbol, static_cast<std::uint8_t>(0)};
            }

            std::uint16_t repeatCount = count / 6;
            auto remainder = static_cast<std::uint8_t>(count % 6);
            if (remainder != 0)
            {
                // We need to repeat at least 3 times. This will steal at most 2 from one of the later repeats, however
                // this is fine as it'll just bump the count from 6 down to 4, which is still valid.
                assert((remainder >= 3) || (repeatCount != 0));
                remainder = std::max(remainder, static_cast<std::uint8_t>(3));
                count -= remainder;
                ++codeLenCounts[16];
                codeLens[codeLenSize++] = {static_cast<std::uint8_t>(16), static_cast<std::uint8_t>(remainder - 3)};
            }

            codeLenCounts[16] += repeatCount;
            while (repeatCount--)
            {
                auto toRepeat = std::min(count, static_cast<std::uint16_t>(6));
                assert(toRepeat >= 3);
                count -= toRepeat;
                codeLens[codeLenSize++] = {static_cast<std::uint8_t>(16), static_cast<std::uint8_t>(toRepeat - 3)};
            }
            assert(count == 0);
        }
    }

    // Convert to a huffman tree & calculate the code lengths of each code length symbol
    max_heap<std::unique_ptr<huffman_tree_node<std::uint8_t>>, huffman_node_compare> codeLenNodes;
    for (std::uint8_t i = 0; i < std::size(codeLenCounts); ++i)
    {
        if (codeLenCounts[i])
        {
            auto node = std::make_unique<huffman_tree_node<std::uint8_t>>();
            node->symbol = i;
            node->count = codeLenCounts[i];
            codeLenNodes.push(std::move(node));
        }
    }

    auto codeLenRoot = build_huffman_tree(codeLenNodes);

    std::uint8_t codeLengthCodeLens[19] = {};
    std::uint8_t maxCodeLength = 0;
    // The order that we write code length codes is not 0...18, but rather a different specified order. This array maps
    // symbols (used as an index) to their location in the output.
    // Position in output:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18
    // Output symbol:      16 17 18  0  8  7  9  6 10  5 11  4 12  3 13  2 14  1 15
    std::uint8_t codeLengthOrder[19] = {3, 17, 15, 13, 11, 9, 7, 5, 4, 6, 8, 10, 12, 14, 16, 18, 0, 1, 2};
    if (!calculate_code_lens(codeLenRoot, 0, codeLengthCodeLens, maxCodeLength, codeLengthOrder))
    {
        return 1;
    }
    auto hclen = static_cast<std::uint8_t>(std::max(maxCodeLength + 1, 4)); // +1 because this is length, but 'codeLengthOrder' is based off indices

    // Figure out the code for each symbol
    std::uint16_t literalLengthCodes[288] = {};
    if (!calculate_codes(literalLengthCodeLens, literalLengthCodes))
    {
        return 1;
    }

    std::uint16_t distanceCodes[32] = {};
    if (!calculate_codes(distanceCodeLens, distanceCodes))
    {
        return 1;
    }

    std::uint16_t codeLengthCodes[19] = {};
    if (!calculate_codes(codeLengthCodeLens, codeLengthCodes))
    {
        return 1;
    }

    // And finally, print out the information
    if (!usingStaticTables)
    {
        std::println("{:0>5b}   # HLIT = {} ({} + 257)", hlit - 257, hlit, hlit - 257);
        std::println("{:0>5b}   # HDIST = {} ({} + 1)", hdist - 1, hdist, hdist - 1);
        std::println("{:0>4b}    # HCLEN = {} ({} + 4)", hclen - 4, hclen, hclen - 4);

        // Print out the alphabet code lengths
        std::println("");
        std::println("# Code Length Alphabet Code Lengths:");
        static constexpr const std::uint8_t codeLengthCodeOrder[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        // NOTE: These are all written as literals. We may need to write more values than 'maxCodeLength', but this should
        // be fine since later values should all have '0' as their length.
        const char* prefix = "";
        for (std::uint8_t i = 0; i < hclen; ++i)
        {
            std::print("{}{:0>3b}", prefix, codeLengthCodeLens[codeLengthCodeOrder[i]]);
            prefix = " ";
        }
        std::println("\n");

        std::println("# Literal/Length & Distance Alphabet Code Lengths:");
        std::println(">>1");
        for (std::size_t i = 0; i < codeLenSize; ++i)
        {
            auto [symbol, extra] = codeLens[i];
            auto len = codeLengthCodeLens[symbol];
            auto code = codeLengthCodes[symbol];
            std::print("{:0>{}b}", code, len);

            switch (symbol)
            {
            default: // 0-15
                assert(extra == 0);
                break;

            case 16:
                assert(extra < 4);
                std::print(" >1 {:0>2b} >>1", extra);
                break;

            case 17:
                assert(extra < 8);
                std::print(" >1 {:0>3b} >>1", extra);
                break;

            case 18:
                assert(extra < 128);
                std::print(" >1 {:0>7b} >>1", extra);
                break;
            }

            std::println("");
        }

        // Print out information about the trees as comments for reference
        std::println("");
        std::println("# Literal/Length Tree:");
        std::println("#   Symbol      Bit Count   Code");
        for (std::uint16_t i = 0; i <= maxLiteralLength; ++i)
        {
            if (literalLengthCodeLens[i])
            {
                std::string text;
                if ((i >= 32) && (i <= 126))
                {
                    text = std::format("'{}'", static_cast<char>(i));
                }
                else if (i == 256)
                {
                    text = "END";
                }
                else
                {
                    text = std::to_string(i);
                }
                std::println(
                    "#   {:<11} {:<11} {:0>{}b}", text, literalLengthCodeLens[i], literalLengthCodes[i], literalLengthCodeLens[i]);
            }
        }

        if (hasDistanceCodes)
        {
            std::println("#");
            std::println("# Distance Tree:");
            std::println("#   Symbol      Bit Count   Code");
            for (std::uint8_t i = 0; i <= maxDistance; ++i)
            {
                if (distanceCodeLens[i])
                {
                    std::println("#   {:<11} {:<11} {:0>{}b}", i, distanceCodeLens[i], distanceCodes[i], distanceCodeLens[i]);
                }
            }
        }
    }

    // And finally, encode the data:
    std::println("");
    std::println("# Encoded Data:");
    std::println(">>1");
    for (const auto& next : outputData)
    {
        switch (next.type)
        {
        case symbol_type::literal_length:
        {
            std::print("{:0>{}b}", literalLengthCodes[next.symbol], literalLengthCodeLens[next.symbol]);
            if (next.symbol <= 256)
            {
                assert(next.extra_data == 0); // Literal or end of block; no extra data
                break;
            }

            auto& info = encoding->lengths[next.symbol - 257];
            assert(next.extra_data <= info.max_extra_data); // Already validated
            if (info.extra_bits > 0)
            {
                std::print(" >1 {:0>{}b} >>1", next.extra_data, info.extra_bits);
            }
            break;
        }

        case symbol_type::distance:
        {
            std::print("{:0>{}b} ", distanceCodes[next.symbol], distanceCodeLens[next.symbol]);
            if (next.symbol <= 3)
            {
                assert(next.extra_data == 0); // No extra data
                break;
            }

            // Otherwise, there is extra data
            auto& info = encoding->distances[next.symbol];
            assert(next.extra_data <= info.max_extra_data); // Already validated
            std::print(">1 {:0>{}b} >>1", next.extra_data, info.extra_bits);
            break;
        }
        }

        std::println("");
    }
}
