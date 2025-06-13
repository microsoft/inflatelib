
# A helper program that, given some input stream of bytes and a code lengths array, produces output encoded using the
# Huffman tree that corresponds to the code lengths array
add_executable(huffman-encode)

target_compile_features(huffman-encode
    PRIVATE
        cxx_std_23
    )

target_sources(huffman-encode
    PRIVATE
        main.cpp
    )
