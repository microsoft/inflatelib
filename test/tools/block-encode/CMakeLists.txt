
# A helper program that, given code/length and distance occurance counts, produces a compressed block header that is
# compatible with the 'bin-write' program
add_executable(block-encode)

target_compile_features(block-encode
    PRIVATE
        cxx_std_23
    )

target_sources(block-encode
    PRIVATE
        main.cpp
    )
