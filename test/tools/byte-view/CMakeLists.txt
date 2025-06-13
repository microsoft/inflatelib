
# A helper program that converts a file into a individual bytes in a format that bin-write can understand
add_executable(byte-view)

target_compile_features(byte-view
    PRIVATE
        cxx_std_23
    )

target_sources(byte-view
    PRIVATE
        main.cpp
    )
