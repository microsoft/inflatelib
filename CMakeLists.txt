cmake_minimum_required(VERSION 3.20)
project(inflatelib
    VERSION 0.1
    DESCRIPTION "A Deflate and Deflate64 decompression library"
    HOMEPAGE_URL "https://github.com/microsoft/inflatelib"
    LANGUAGES C CXX
    )

option(INFLATELIB_TEST "Build tests for local development and CI validation" ON)
option(INFLATELIB_ASAN "Build with Address Sanitizer" OFF)
option(INFLATELIB_UBSAN "Build with Undefined Behavior Sanitizer" OFF)
option(INFLATELIB_FUZZ "Build the fuzzing target" OFF)

# Fix the CMake defaults. See: https://gitlab.kitware.com/cmake/cmake/-/issues/20812
if (INFLATELIB_TEST)
    if (MSVC)
        # /Ob1 disables inlining for functions not marked inline. This is not desired except for minimum size
        string(REPLACE "/Ob1" "/Ob2" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/Ob1" "/Ob2" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

        # Don't use /INCREMENTAL for RelWithDebInfo
        string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO}")
        string(REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")

        # We always want debug info
        string(APPEND CMAKE_C_FLAGS_RELEASE " /Zi")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
        string(APPEND CMAKE_C_FLAGS_MINSIZEREL " /Zi")
        string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " /Zi")

        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /debug")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS_RELEASE " /debug")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /debug")
        string(APPEND CMAKE_EXE_LINKER_FLAGS_MINSIZEREL " /debug")
        string(APPEND CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL " /debug")
        string(APPEND CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL " /debug")

        # TODO: /Gy?
    endif()

    # Max warning levels/set warnings as errors
    if (MSVC)
        # MSVC-like command syntax
        add_compile_options(/W4 /WX)

        if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            # MSVC-specific options
            add_compile_options(/permissive-)
        else()
            # Assume clang-cl
            add_compile_options(
                -fno-delayed-template-parsing
                -fno-ms-compatibility
                )
        endif()
    else()
        # Assume GNU-like command syntax
        add_compile_options(-Wall -Wextra -pedantic -Werror)
    endif()

    # Handle sanitizer options
    set(SANITIZER_FLAGS)
    set(SANITIZER_LINKER_FLAGS)
    set(SANITIZER_LIBS)
    if ((CMAKE_C_COMPILER_ID MATCHES "Clang") OR (CMAKE_C_COMPILER_ID MATCHES "GNU"))
        if (INFLATELIB_ASAN OR INFLATELIB_FUZZ)
            list(APPEND SANITIZER_FLAGS -fsanitize=address)

            # Clang + ASan on Windows curerntly has some limitations. We try and manually handle those limitations here as
            # opposed to difficult to track down and understand failures later on.
            if (WIN32)
                if (CMAKE_BUILD_TYPE MATCHES "Debug")
                    message(FATAL_ERROR "Clang does not currently support linking with debug runtime libraries with Address Sanitizer enabled on Windows")
                else()
                    if ($ENV{Platform} STREQUAL "x86")
                        list(APPEND SANITIZER_LIBS
                            clang_rt.asan_dynamic-i386
                            clang_rt.asan_dynamic_runtime_thunk-i386.lib
                            )
                    elseif ($ENV{Platform} STREQUAL "x64")
                        list(APPEND SANITIZER_LIBS
                            clang_rt.asan_dynamic-x86_64.lib
                            clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
                            )
                    else()
                        message(FATAL_ERROR "Address Sanitizer not supported on $ENV{Platform}")
                    endif()
                endif()
            else()
                list(APPEND SANITIZER_LINKER_FLAGS -fsanitize=address)
            endif()

            if (INFLATELIB_FUZZ)
                # Fuzzing requires a few extra flags
                list(APPEND SANITIZER_FLAGS
                    -fsanitize-coverage=inline-8bit-counters
                    -fsanitize-coverage=edge
                    -fsanitize-coverage=trace-cmp
                    -fsanitize-coverage=trace-div
                    )

                if (WIN32)
                    list(APPEND SANITIZER_LIBS libsancov.lib)
                    list(APPEND SANITIZER_FLAGS /MT)
                    if ($ENV{Platform} STREQUAL "x86")
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MT-i386)
                    elseif ($ENV{Platform} STREQUAL "x64")
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MT-x86_64.lib)
                    endif()
                    # No 'else' clause because all paths handled above for ASan
                elseif (CMAKE_C_COMPILER_ID MATCHES "GNU")
                    message(FATAL_ERROR "GCC does not currently support fuzzing")
                else()
                    list(APPEND SANITIZER_LINKER_FLAGS -fsanitize=fuzzer)
                endif()
            endif()
        endif()
        if (INFLATELIB_UBSAN)
            list(APPEND SANITIZER_FLAGS -fsanitize=undefined)

            # Similar to ASan, UBSan has issues on Windows with Debug since the *ubsan_standalone* lib seems to have been
            # built for non-debug
            if (WIN32)
                if (CMAKE_BUILD_TYPE MATCHES "Debug")
                    message(FATAL_ERROR "Clang does not currently support linking with debug runtime libraries with Undefined Behavior Sanitizer enabled on Windows")
                else()
                    list(APPEND SANITIZER_FLAGS /MT)
                endif()
            else()
                list(APPEND SANITIZER_LINKER_FLAGS -fsanitize=undefined)
            endif()
        endif()
    elseif (CMAKE_C_COMPILER_ID MATCHES "MSVC")
        if (INFLATELIB_ASAN OR INFLATELIB_FUZZ)
            list(APPEND SANITIZER_FLAGS /fsanitize=address)

            # Fuzzing requires a few extra flags
            if (INFLATELIB_FUZZ)
                list(APPEND SANITIZER_FLAGS
                    /fsanitize-coverage=inline-8bit-counters
                    /fsanitize-coverage=edge
                    /fsanitize-coverage=trace-cmp
                    /fsanitize-coverage=trace-div
                    )
                list(APPEND SANITIZER_LIBS sancov.lib)

                if ($ENV{Platform} STREQUAL "x86")
                    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MDd-i386)
                    else()
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MD-i386)
                    endif()
                elseif ($ENV{Platform} STREQUAL "x64")
                    if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MDd-x86_64.lib)
                    else()
                        list(APPEND SANITIZER_LIBS clang_rt.fuzzer_MD-x86_64.lib)
                    endif()
                else()
                    message(FATAL_ERROR "Fuzzing not supported on $ENV{Platform}")
                endif()
            endif()
        endif()
        if (INFLATELIB_UBSAN)
            message(FATAL_ERROR "MSVC does not support Undefined Behavior Sanitizer")
        endif()
    else()
        if (INFLATELIB_ASAN)
            message(FATAL_ERROR "Address Sanitizer support is not configured for this compiler")
        elseif (INFLATELIB_UBSAN)
            message(FATAL_ERROR "Undefined Behavior Sanitizer support is not configured for this compiler")
        endif()
    endif()
endif()

add_subdirectory(src)

if (INFLATELIB_TEST)
    add_subdirectory(test)
endif()

# CPack
set(CPACK_PACKAGE_VENDOR "Microsoft Corporation")
set(CPACK_GENERATOR "ZIP;TGZ;TBZ2;TXZ;7Z")
include(CPack)
