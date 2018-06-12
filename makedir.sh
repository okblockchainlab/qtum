#!/usr/bin/env bash

mkdir -p build
cd build
ln -snvf ../src/qtum-cli cli
ln -snvf ../src/qtum-tx tx
ln -snvf ../src/qtumd qtumd
