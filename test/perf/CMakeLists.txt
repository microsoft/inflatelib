
add_executable(perftests)

target_link_libraries(perftests
    PRIVATE
        inflatelib
    )

target_sources(perftests
    PRIVATE
        main.c
        algorithms.c
        file_io.c
        histogram.c
    )
