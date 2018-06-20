#!/bin/bash
EXT=so
PROJECT_NAME=qtum
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

rm ${PROJECT_NAME}d.${EXT}
make ${PROJECT_NAME}d.${EXT} -f Makefile.so.include -j 6 V=1

rm ${PROJECT_NAME}rpc
make ${PROJECT_NAME}rpc -f Makefile.so.include -j 6 V=1

