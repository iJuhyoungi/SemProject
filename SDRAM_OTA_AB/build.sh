#!/bin/bash

rm -rf build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-toolchain.cmake
cmake --build build
