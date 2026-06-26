
add_subdirectory(corpus)

add_executable(fuzz-inflate)

target_compile_features(fuzz-inflate
    PRIVATE
        cxx_std_23
    )

target_link_libraries(fuzz-inflate
    PRIVATE
        inflatelib::inflatelib
    )

target_sources(fuzz-inflate
    PRIVATE
        main.cpp
    )
