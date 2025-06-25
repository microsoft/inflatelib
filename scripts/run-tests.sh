#!/bin/bash -e

rootDir="$(cd "$(dirname "$0")/.." && pwd)"
buildRoot="$rootDir/build"

# Check to see if this is WSL. If it is, build output to go into a separate directory so that build output does not collide with
# Windows build output
if "$rootDir/scripts/check-wsl.sh"; then
    buildRoot="$buildRoot/wsl"
fi

architectures=
case $("$rootDir/scripts/host-arch.sh") in
    x86)
        architectures=(x86)
        ;;
    x64)
        architectures=(x86 x64)
        ;;
    arm)
        architectures=(arm)
        ;;
    arm64)
        architectures=(arm arm64)
        ;;
    *)
        echo "ERROR: Unknown architecture $(uname -m)"
        exit 1
        ;;
esac

for compiler in gcc clang; do
    for buildType in debug release relwithdebinfo minsizerel; do
        # NOTE: Fuzzing explicitly left off since we don't build tests for it
        for sanitizer in none asan ubsan; do
            for arch in "${architectures[@]}"; do
                suffix=""
                if [ "$sanitizer" != "none" ]; then
                    suffix="-${sanitizer}"
                fi

                buildDir="$buildRoot/$compiler$arch$buildType$suffix"
                if [ -f "$buildDir/test/cpp/cpptests" ]; then
                    echo "Running tests from '$buildDir'"
                    "$buildDir/test/cpp/cpptests"
                fi
            done
        done
    done
done
