#!/bin/bash -e

rootDir="$(cd "$(dirname "$0")/.." && pwd)"
buildRoot="$rootDir/build"

# Check to see if this is WSL. If it is, we want build output to go into a separate directory so that build output does
# not 
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
        for arch in "${architectures[@]}"; do
            buildDir="$buildRoot/$compiler$arch$buildType"
            if [ -d "$buildDir" ]; then
                echo "Building from '$buildDir'"
                cmake --build "$buildDir"
            fi
        done
    done
done
