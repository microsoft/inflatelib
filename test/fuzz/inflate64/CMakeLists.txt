
add_subdirectory(corpus)

add_executable(fuzz-inflate64)

target_link_libraries(fuzz-inflate64
    PRIVATE
        inflatelib::inflatelib
    )

target_sources(fuzz-inflate64
    PRIVATE
        main.cpp
    )
