
add_executable(perftests)

find_package(ZLIB REQUIRED)
target_link_libraries(perftests
    PRIVATE
        inflatelib
        ZLIB::ZLIB
    )

target_sources(perftests
    PRIVATE
        main.c
        algorithms.c
        file_io.c
        histogram.c
    )
