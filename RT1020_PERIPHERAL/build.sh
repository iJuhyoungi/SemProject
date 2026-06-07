#!/bin/bash

set -e      # 에러 발생 시 스크립트 종료

rm -rf build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-toolchain.cmake
cmake --build build
