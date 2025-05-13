
add_library(inflatelib)

target_compile_features(inflatelib
    PRIVATE
        c_std_11
    )

target_include_directories(inflatelib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

target_sources(inflatelib
    PRIVATE
        bitstream.c
        huffman_tree.c
        inflate.c
        window.c
    )
