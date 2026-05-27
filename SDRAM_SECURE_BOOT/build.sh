#!/bin/bash

set -e      # 에러 발생 시 스크립트 종료

python3 tools/extract_pubkey.py

rm -rf build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi-toolchain.cmake
cmake --build build
