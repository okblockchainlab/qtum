#!/usr/bin/env bash

if [ -z "$JAVA_HOME" ]; then
	printf "No JAVA_HOME detected! "
	printf "Setup JAVA_HOME before build: export JAVA_HOME=/path/to/java\\n"
	exit 1
fi

git submodule init && git submodule update

./autogen.sh

./configure --with-gui=no --enable-debug -disable-gui-tests --disable-bench

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

cd src
make qtumd.${EXT} -f Makefile.qtumd.so -j 6 V=1
nm qtumd.${EXT} |grep okcoin
