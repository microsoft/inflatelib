
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
    std::printf("USAGE:\n");
    std::printf("  bin-write <input_file> <output_file>\n");
}
