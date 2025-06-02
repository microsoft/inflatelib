
add_executable(cpptests)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(cpptests
    PRIVATE
        inflatelib
        Catch2::Catch2WithMain
        ${SANITIZER_LIBS}
    )

target_include_directories(cpptests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib
    )

target_compile_features(cpptests
    PRIVATE
        cxx_std_23
    )

target_compile_options(cpptests
    PRIVATE
        ${SANITIZER_FLAGS}
    )

target_link_options(cpptests
    PRIVATE
        ${SANITIZER_LINKER_FLAGS}
    )

target_sources(cpptests
    PRIVATE
        BitstreamTests.cpp
        HuffmanTreeTests.cpp
        InflateTests.cpp
        WindowTests.cpp
    )
