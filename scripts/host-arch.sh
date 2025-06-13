#!/bin/bash -e

case $(uname -m) in
    x86_64)
        echo "x64"
        ;;
    i386 | i686)
        echo "x86"
        ;;
    arm | armv7l)
        echo "arm"
        ;;
    aarch64)
        echo "arm64"
        ;;
    *)
        echo "ERROR: Unknown architecture $(uname -m)"
        exit 1
        ;;
esac
