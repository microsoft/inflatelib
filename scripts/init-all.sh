#!/bin/bash -e

scriptDir=$(cd "$(dirname "$0")" && pwd)

for compiler in gcc clang; do
    for buildType in debug release relwithdebinfo minsizerel; do
        $scriptDir/init.sh -c $compiler -b $buildType
    done
done
