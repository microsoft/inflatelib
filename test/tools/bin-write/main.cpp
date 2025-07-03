/*
 *    Copyright (c) Microsoft. All rights reserved.
 *    This code is licensed under the MIT License.
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 *    ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 *    TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 *    PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <cstdio>
#include <string_view>

#include "parser.h"

using namespace std::literals;

void print_usage();

int main(int argc, char** argv)
{
    if ((argc == 2) && (argv[1] == "help"sv))
    {
        print_usage();
        return 0;
    }

    if (argc != 3)
    {
        std::printf("ERROR: Must specify exactly one input file and one output file\n");
        return 1;
    }

    parser p;
    if (!p.parse(argv[1]) || !p.write_to_file(argv[2]))
    {
        return 1;
    }

    return 0;
}

void print_usage()
{
    std::println(R"^-^(
USAGE
    bin-write <input-path> <output-path>

DESCRIPTION
    Converts 'input-path' to the binary file 'output-path'. See the 'grammar.md' for more information on the grammar
    used to parse the input file.

ARGUMENTS
    input-path   The path to the input file.
    output-path  The path to the output file.
)^-^");
}
