#!/bin/sh

rm -rf build
mkdir -p build
(
    cd build
    cmake ..
    cmake --build .
    make package
)

./makezip.sh
