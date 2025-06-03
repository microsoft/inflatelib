set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

# ASan + Clang on Windows is not yet compatible with Debug libraries
set(VCPKG_BUILD_TYPE release)

# TODO: VCPkg ~~destroys~~ cleans up the environment variables, which includes PATH, so there's no great way to reliably
# find the LLVM installation path... It might be worth setting an environment variable & explicitly passing it through
find_program(CLANG_CL_PATH NAMES "clang-cl" "clang-cl.exe" PATHS "$ENV{ProgramFiles}/llvm/bin" REQUIRED)

set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCLANG_CL_PATH=${CLANG_CL_PATH}")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../../toolchain/windows-llvm.cmake")
set(VCPKG_LOAD_VCVARS_ENV ON)

string(APPEND VCPKG_C_FLAGS " -fsanitize=address -m32")
string(APPEND VCPKG_CXX_FLAGS " -fsanitize=address -m32")

# TODO: This is somewhat of a hack just to get CMake configuration to pass... Ideally we can pass '-fsanitize=address'
# _only_ when building Catch2 (since we don't need to link anything), however it's not quite clear if that's possible
set(VCPKG_LINKER_FLAGS "clang_rt.asan_dynamic-i386.lib clang_rt.asan_dynamic_runtime_thunk-i386.lib")
