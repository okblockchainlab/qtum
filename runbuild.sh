#!/usr/bin/env bash

#./configure --with-gui=no --enable-debug --disable-tests -disable-gui-tests --disable-bench

git submodule init && git submodule update
make -j 6 V=1
