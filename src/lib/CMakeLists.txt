
add_library(inflatelib)

target_compile_features(inflatelib
    PRIVATE
        c_std_11
    )

target_compile_options(inflatelib
    PUBLIC
        ${SANITIZER_FLAGS}
    )

target_link_libraries(inflatelib
    INTERFACE
        ${SANITIZER_LIBS}
    )

target_link_options(inflatelib
    INTERFACE
        ${SANITIZER_LINKER_FLAGS}
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
