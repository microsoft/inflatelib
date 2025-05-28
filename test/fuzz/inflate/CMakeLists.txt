
add_executable(fuzz-inflate)

target_link_libraries(fuzz-inflate
    PRIVATE
        inflatelib
    )

target_sources(fuzz-inflate
    PRIVATE
        main.cpp
    )
