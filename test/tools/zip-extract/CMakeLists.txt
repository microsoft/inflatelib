
# A helper program that, given code/length and distance occurance counts, produces a compressed block header that is
# compatible with the 'bin-write' program
add_executable(zip-extract)

target_compile_features(zip-extract
    PRIVATE
        cxx_std_23
    )

target_sources(zip-extract
    PRIVATE
        main.cpp
    )
