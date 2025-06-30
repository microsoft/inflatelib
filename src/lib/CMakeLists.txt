
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

add_library(inflatelib)
add_library(inflatelib::inflatelib ALIAS inflatelib)

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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    )

target_sources(inflatelib
    PRIVATE
        bitstream.c
        huffman_tree.c
        inflate.c
        window.c
    )

# Installation
install(
    TARGETS inflatelib
    EXPORT inflatelib_export
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

install(
    DIRECTORY ../include/
    TYPE INCLUDE
    )

install(
    EXPORT inflatelib_export
    FILE inflatelib-config.cmake
    NAMESPACE inflatelib::
    DESTINATION cmake
    )

write_basic_package_version_file(
    inflatelib-config-version.cmake
    COMPATIBILITY SameMajorVersion
    )
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/inflatelib-config-version.cmake"
    DESTINATION cmake
    )
