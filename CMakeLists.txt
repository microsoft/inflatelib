cmake_minimum_required(VERSION 3.20)
project(inflatelib)

# Fix the CMake defaults. See: https://gitlab.kitware.com/cmake/cmake/-/issues/20812
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

add_subdirectory(src)
add_subdirectory(test)
