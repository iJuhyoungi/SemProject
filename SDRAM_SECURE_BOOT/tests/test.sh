#!/bin/bash


#ctest --test-dir build --output-on-failure

set -e
cd "$(dirname "$0")"
ctest --test-dir build -V                 # 항상 verbose
# 또는
#ctest --test-dir build --output-on-failure  # 실패 시에만 stdout 노출


