
add_subdirectory(corpus)

add_executable(fuzz-inflate)

target_link_libraries(fuzz-inflate
    PRIVATE
        inflatelib::inflatelib
    )

target_sources(fuzz-inflate
    PRIVATE
        main.cpp
    )
