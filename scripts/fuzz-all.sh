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

# Default to 10 min unless otherwise specified
timeout=600
if [ -n "$1" ]; then
    timeout=$1
fi

for compiler in clang; do # NOTE: Only Clang currently supports fuzzing
    for buildType in debug release relwithdebinfo minsizerel; do
        for arch in "${architectures[@]}"; do
            buildDir="$buildRoot/$compiler$arch$buildType-fuzz"
            echo "Fuzzing from '$buildDir'"
            if [ -f "$buildDir/test/fuzz/inflate/fuzz-inflate" ]; then
                mkdir -p "$buildDir/test/fuzz/inflate/corpus-out"
                "$buildDir/test/fuzz/inflate/fuzz-inflate" -max_total_time=$timeout "$buildDir/test/fuzz/inflate/corpus-out" "$buildDir/test/fuzz/inflate/corpus-in"
            fi
            if [ -f "$buildDir/test/fuzz/inflate64/fuzz-inflate64" ]; then
                mkdir -p "$buildDir/test/fuzz/inflate64/corpus-out"
                "$buildDir/test/fuzz/inflate64/fuzz-inflate64" -max_total_time=$timeout "$buildDir/test/fuzz/inflate64/corpus-out" "$buildDir/test/fuzz/inflate64/corpus-in"
            fi
        done
    done
done
