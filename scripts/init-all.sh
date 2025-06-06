#!/bin/bash -e

scriptDir="$(cd "$(dirname "$0")" && pwd)"

for compiler in gcc clang; do
    for buildType in debug release relwithdebinfo minsizerel; do
        for sanitizer in none address undefined fuzz; do
            extraArgs=""
            if [ "$sanitizer" == "fuzz" ]; then
                extraArgs="-f"
                if [ "$compiler" == "gcc" ]; then
                    continue # GCC does not support fuzzing at the moment
                fi
            elif [ "$sanitizer" != "none" ]; then
                extraArgs="-s $sanitizer"
            fi
            "$scriptDir/init.sh" -c $compiler -b $buildType $extraArgs
        done
    done
done
