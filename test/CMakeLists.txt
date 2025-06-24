
add_subdirectory(tools)

# If the target and host architectures are not compatible (e.g. compiling for ARM on an x64 machine), we cannot run the
# bin-write executable. This is okay since we can't actually run the tests that depend on this data anyway
# TODO: For now, the only scenario we expect to encounter this is on Windows when building on an AMD64 machine,
# cross-compiling for ARM/ARM64. Ideally this could be made more general, however there doesn't appear to be an easy and
# portable way to do this in CMake.
set(BUILD_TEST_FILES TRUE)

if (DEFINED ENV{Platform} AND DEFINED ENV{PROCESSOR_ARCHITECTURE})
    if (("$ENV{Platform}" MATCHES "arm.*") AND NOT ("$ENV{PROCESSOR_ARCHITECTURE}" MATCHES "arm.*"))
        set(BUILD_TEST_FILES FALSE)
    endif()
endif()

if (BUILD_TEST_FILES)
    add_subdirectory(data)
endif()

if (NOT INFLATELIB_FUZZ)
    add_subdirectory(cpp)

    # The perf tests are pretty useless when sanitizers are enabled. Note that the tests will consume the same input as the
    # perf tests, so it's not like we're missing potential coverage here.
    if (NOT (INFLATELIB_ASAN OR INFLATELIB_UBSAN))
        add_subdirectory(perf)
    endif()
else()
    add_subdirectory(fuzz)
endif()
