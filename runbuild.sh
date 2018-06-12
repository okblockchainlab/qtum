#!/usr/bin/env bash

./autogen.sh

./configure --with-gui=no --enable-debug -disable-gui-tests --disable-bench

git submodule init && git submodule update
#make -j 6 V=1

cd src
make qtumd.so -f Makefile.qtumd.so -j 6 V=1
nm qtumd.so |grep okcoin

make qtumd.dylib -f Makefile.qtumd.so -j 6 V=1
nm qtumd.dylib |grep okcoin
