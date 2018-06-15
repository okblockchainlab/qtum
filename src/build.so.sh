#!/bin/bash
EXT=so

TARGET_OS=`uname -s`

case "$TARGET_OS" in
    Darwin)
        EXT=dylib
        ;;
    Linux)
        EXT=so
        ;;
    *)
        echo "Unknown platform!" >&2
        exit 1
esac
rm qtumd.${EXT}
make qtumd.${EXT} -f Makefile.qtumd.so -j 6 V=1

