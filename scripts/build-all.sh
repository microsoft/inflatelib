#!/bin/bash -e

rootDir=$(cd "$(dirname "$0")/.." && pwd)
buildRoot="$rootDir/build"

# Check to see if this is WSL. If it is, we want build output to go into a separate directory so that build output does
# not 
if $($rootDir/scripts/check-wsl.sh); then
    buildRoot="$buildRoot/wsl"
fi

# TODO: Architecture
architectures=(x86 x64)

for compiler in gcc clang; do
    for buildType in debug release relwithdebinfo minsizerel; do
        for arch in "${architectures[@]}"; do
            buildDir=$buildRoot/$compiler$arch$buildType
            if [ -d $buildDir ]; then
                echo "Building from $buildDir"
                pushd $buildDir > /dev/null
                ninja
                popd > /dev/null
            fi
        done
    done
done
