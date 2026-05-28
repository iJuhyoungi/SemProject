#!/bin/bash

# Usage: ./flash_mcu.sh [target]
#   (no arg)   = all (default)
#   all        = chip erase + flash stage1 + stage2
#   stage1     = Stage 1 (immutable verifier) 만
#   stage2     = Stage 2 (검증 대상 image) 만
#   app_a      = App A (0x60048000)
#   app_b      = App B (0x60088000)
#   apps       = App A + App B

# 메모리 맵:
#   Stage 1  -> 0x60000000  (FCB + IVT + verify 코드, 24KB region)
#   Stage 2  -> 0x60008000  (서명된 image, 256KB region)

TARGET="${1:-all}"

# pyOCD builtin target. pack target (mimxrt1021dag5a) 은 0.43.1 의 NXP CMSIS
# Pack debug sequence 에 unaligned read AssertionError 가 있어 fail. builtin 우회.
DEV="-t mimxrt1020"

erase_chip() {
    echo "========================================"
    echo " 1. Erasing entire Flash Memory..."
    echo "========================================"
    pyocd erase $DEV --chip
    echo ""
}

flash_stage1() {
    echo "========================================"
    echo " Flashing Stage 1 (0x60000000)"
    echo "========================================"
    # Stage 1 immutable. FCB + IVT + verify 코드. 0x60000000 부터.
    pyocd flash build/bootloader/stage1/bootloader_stage1.bin $DEV --base-address 0x60000000
    echo ""
}

flash_stage2() {
    echo "========================================"
    echo " Flashing Stage 2 (0x60008000)"
    echo "========================================"
    # Stage 2 = Stage 1 이 검증 후 점프하는 대상. 0x60008000 부터.
    pyocd flash build/bootloader/stage2/bootloader_stage2.bin $DEV --base-address 0x60008000
    echo ""
}

flash_app_a() {
    echo "========================================"
    echo " Flashing App A (0x60048000)"
    echo "========================================"
    pyocd flash build/app/app_a/app_a.bin $DEV --base-address 0x60048000
    echo ""
}

flash_app_b() {
    echo "========================================"
    echo " Flashing App B (0x60088000)"
    echo "========================================"
    pyocd flash build/app/app_b/app_b.bin $DEV --base-address 0x60088000
    echo ""
}

case "$TARGET" in
    all)
        erase_chip
        flash_stage1
        flash_stage2
        flash_app_a
        flash_app_b
        ;;
    stage1)
        flash_stage1
        ;;
    stage2|boot)
        flash_stage2
        ;;
    app_a)  flash_app_a ;;
    app_b)  flash_app_b ;;
    apps)   flash_app_a; flash_app_b ;;
    
    *)
        echo "usage: $0 [all|stage1|stage2|app_a|app_b|apps]   (default: all)"
        exit 1
        ;;
esac

echo "✅ All done! Please reset the board."
