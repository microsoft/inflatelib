#!/bin/bash

rootDir="$(cd "$(dirname "$0")/.." && pwd)"
buildRoot="$rootDir/build"

# Check to see if this is WSL. If it is, we want build output to go into a separate directory so that build output does
# not
if "$rootDir/scripts/check-wsl.sh"; then
    buildRoot="$buildRoot/wsl"
fi

compiler=
generator=
buildType=
cmakeArgs=()
vcpkgRoot=
sanitizer=
fuzz=

function show_help {
    echo "USAGE:"
    echo "    init.sh [-c <compiler>] [-b <build_type>] [-g <generator>] [-s <sanitizer>] [-f] [-p <path-to-vcpkg-root>]"
    echo
    echo "ARGUMENTS:"
    echo "    -c      Spcifies the compiler to use, either 'gcc' (the default) or 'clang'"
    echo "    -b      Specifies the value of 'CMAKE_BUILD_TYPE', either 'debug' (the default),"
    echo "            'release', 'relwithdebinfo', or 'minsizerel'"
    echo "    -g      Specifies the generator to use, either 'ninja' (the default) or 'make'"
    echo "    -s      Specifies the sanitizer to use, either 'address' or 'undefined'. If this value is not"
    echo "            specified, then no sanitizer will be used. This argument is not compatible with '-f'"
    echo "    -f      When set, builds the fuzzing targets. This argument is not compatible with '-s'"
    echo "    -p      Specifies the path to the root of your local vcpkg clone. If this value is not"
    echo "            specified, then several attempts will be made to try and deduce it. The first attempt"
    echo "            will be to check for the presence of the VCPKG_ROOT environment variable"
}

while getopts hc:b:g:s:fp: opt; do
    arg=${OPTARG,,}
    case $opt in
        h)
            show_help
            exit 0
            ;;
        c)
            if [ "$compiler" != "" ]; then
                echo "Error: Compiler already specified. Cannot specify more than one compiler."
                exit 1
            fi
            if [ $arg == "gcc" ]; then
                compiler="gcc"
            elif [ $arg == "clang" ]; then
                compiler="clang"
            else
                echo "Error: Invalid compiler specified. Must be either 'gcc' or 'clang'."
                exit 1
            fi
            ;;
        b)
            if [ "$buildType" != "" ]; then
                echo "Error: Build type already specified. Cannot specify more than one build type."
                exit 1
            fi
            if [ $arg == "debug" ]; then
                buildType="debug"
            elif [ $arg == "release" ]; then
                buildType="release"
            elif [ $arg == "relwithdebinfo" ]; then
                buildType="relwithdebinfo"
            elif [ $arg == "minsizerel" ]; then
                buildType="minsizerel"
            else
                echo "Error: Invalid build type specified. Must be either 'debug', 'release', 'relwithdebinfo', or 'minsizerel'."
                exit 1
            fi
            ;;
        g)
            if [ "$generator" != "" ]; then
                echo "Error: Generator already specified. Cannot specify more than one generator."
                exit 1
            fi
            if [ $arg == "ninja" ]; then
                generator="ninja"
            elif [ $arg == "make" ]; then
                generator="make"
            else
                echo "Error: Invalid generator specified. Must be either 'ninja' or 'make'."
                exit 1
            fi
            ;;
        s)
            if [ "$sanitizer" != "" ]; then
                echo "Error: Sanitizer already specified. Cannot specify more than one sanitizer."
                exit 1
            fi
            if [ "$fuzz" != "" ]; then
                echo "Error: Cannot specify both '-s' and '-f'."
                exit 1
            fi
            if [ $arg == "address" ]; then
                sanitizer="asan"
            elif [ $arg == "undefined" ]; then
                sanitizer="ubsan"
            else
                echo "Error: Invalid sanitizer specified. Must be either 'address' or 'undefined'."
                exit 1
            fi
            ;;
        f)
            if [ "$fuzz" != "" ]; then
                echo "Error: Fuzzing already specified."
                exit 1
            fi
            if [ "$sanitizer" != "" ]; then
                echo "Error: Cannot specify both '-s' and '-f'."
                exit 1
            fi
            fuzz=1
            ;;
        p)
            if [ "$vcpkgRoot" != "" ]; then
                echo "Error: VCPKG_ROOT already specified. Cannot specify more than one vcpkg root."
                exit 1
            fi
            vcpkgRoot=$OPTARG
            ;;
    esac
done

# Select the defaults
if [ "$compiler" == "" ]; then
    compiler="gcc"
fi
if [ "$buildType" == "" ]; then
    buildType="debug"
fi
if [ "$generator" == "" ]; then
    generator="ninja"
fi
if [ "$vcpkgRoot" == "" ]; then
    if [ "$VCPKG_ROOT" != "" ]; then
        vcpkgRoot="$(realpath "$VCPKG_ROOT")"
    else
        # Try and find vcpkg in the PATH
        vcpkgPath="$(/bin/which vcpkg)"
        if [ $? == 0 ]; then
            vcpkgRoot="$(dirname "$(realpath "$vcpkgPath")")"
        fi
    fi
fi
if [ "$vcpkgRoot" == "" ]; then
    echo "ERROR: Unable to deduce the path to vcpkg"
    show_help
    exit 1
fi

# Build the command line

# GCC is the normally default, however this can be overridden by setting the 'CC' and 'CXX' environment variables, so
# we always explicitly set the compiler here
if [ "$compiler" == "gcc" ]; then
    cmakeArgs+=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
elif [ "$compiler" == "clang" ]; then
    cmakeArgs+=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
fi

# Makefiles are the default with CMake
if [ "$generator" == "ninja" ]; then
    cmakeArgs+=(-G Ninja)
fi

if [ "$buildType" == "debug" ]; then
    cmakeArgs+=(-DCMAKE_BUILD_TYPE=Debug)
elif [ "$buildType" == "release" ]; then
    cmakeArgs+=(-DCMAKE_BUILD_TYPE=Release)
elif [ "$buildType" == "relwithdebinfo" ]; then
    cmakeArgs+=(-DCMAKE_BUILD_TYPE=RelWithDebInfo)
elif [ "$buildType" == "minsizerel" ]; then
    cmakeArgs+=(-DCMAKE_BUILD_TYPE=MinSizeRel)
fi

suffix=
if [ "$sanitizer" == "asan" ]; then
    cmakeArgs+=(-DINFLATELIB_ASAN=ON)
    suffix="-asan"
elif [ "$sanitizer" == "ubsan" ]; then
    cmakeArgs+=(-DINFLATELIB_UBSAN=ON)
    suffix="-ubsan"
elif [ $fuzz ]; then
    cmakeArgs+=(-DINFLATELIB_FUZZ=ON)
    suffix="-fuzz"
fi

# TODO: Figure out how to cross-compile reliably. For now, just support the machine architecture
arch=$("$rootDir/scripts/host-arch.sh")
if [ $? != 0 ]; then
    echo "ERROR: Unable to determine the host architecture"
    exit 1
fi

cmakeArgs+=("-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# Create the build directory if it doesn't exist
buildDir="$buildRoot/$compiler$arch$buildType$suffix"
mkdir -p "$buildDir"

# Run CMake
echo "Using compiler....... $compiler"
echo "Using architecture... $arch"
echo "Using build type..... $buildType"
echo "Using build root..... $buildDir"
cmake -S "$rootDir" -B "$buildDir" "${cmakeArgs[@]}"
