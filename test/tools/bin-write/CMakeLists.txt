
# A program that, given a text file that describes how to write bytes, produces a corresponding binary file. See the
# grammar.md file for a description of the input file format
add_executable(bin-write)

target_compile_features(bin-write
    PRIVATE
        cxx_std_23
    )

target_sources(bin-write
    PRIVATE
        lexer.cpp
        main.cpp
        parser.cpp
    )
