#!/bin/bash
set -e   # 에러 시 즉시 중단

# pyOCD builtin target. pack target(mimxrt1021dag5a)은 0.43.1 NXP CMSIS Pack
# debug sequence 의 unaligned read AssertionError 로 fail → builtin 우회.
DEV="-t mimxrt1020"
BIN="build/rt1020_fls.bin"

if [ ! -f "$BIN" ]; then
    echo "❌ $BIN 없음. 먼저 ./build.sh 로 빌드하세요."
    exit 1
fi

echo "==== 1) Chip Erase ===="
pyocd erase $DEV --chip

echo "==== 2) Flash $BIN @ 0x60000000 ===="
# objcopy 가 최저 LMA(FCB, 0x60000000)부터 한 덩어리로 뜬 raw 이미지.
# FCB + IVT + .text 가 이미 한 blob 이므로 0x60000000 에 통째로 굽는다.
pyocd flash "$BIN" $DEV --base-address 0x60000000
