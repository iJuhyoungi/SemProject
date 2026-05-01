#!/bin/bash

# Usage: ./flash_mcu.sh [target]
#   (no arg)   = all (default, backward compatible)
#   all        = chip erase + flash 4 images
#   metadata   = boot_ctrl 만 (Slot trial 시나리오 재진입용)
#   boot       = bootloader 만
#   a          = app_slot_a 만
#   b          = app_slot_b 만
#   apps       = a + b

TARGET="${1:-all}"
# pyOCD builtin target. pack target (mimxrt1021dag5a) 은 0.43.1 의
# NXP CMSIS Pack debug sequence 에 unaligned read AssertionError 가 있어
# erase/flash/halt 가 fail. builtin 은 우회 가능.
DEV="-t mimxrt1020"

erase_chip() {
    echo "========================================"
    echo " 1. Erasing entire Flash Memory..."
    echo "========================================"
    pyocd erase $DEV --chip
    echo ""
}

flash_bootloader() {
    echo "========================================"
    echo " Flashing BOOTLOADER (0x60000000)"
    echo "========================================"
    # bin + base-address 패턴. elf 로 굽으면 .flash_config (FCB) 섹션이
    # 누락되어 boot ROM 이 FCB 매직 검증 fail → 보드 stuck.
    pyocd flash build/bootloader/bootloader.bin $DEV --base-address 0x60000000
    echo ""
}

flash_metadata() {
    echo "========================================"
    echo " Flashing BOOT CTRL (0x603F0000)"
    echo "========================================"
    pyocd flash build/bootctrl/boot_ctrl.bin $DEV --base-address 0x603F0000
    echo ""
}

flash_a() {
    echo "========================================"
    echo " Flashing APP SLOT A (0x60040000)"
    echo "========================================"
    pyocd flash build/app_slot_a/app_slot_a.bin $DEV --base-address 0x60040000
    echo ""
}

flash_b() {
    echo "========================================"
    echo " Flashing APP SLOT B (0x60200000)"
    echo "========================================"
    pyocd flash build/app_slot_b/app_slot_b.bin $DEV --base-address 0x60200000
    echo ""
}

case "$TARGET" in
    all)
        erase_chip
        flash_bootloader
        flash_metadata
        flash_a
        flash_b
        ;;
    metadata)
        flash_metadata
        ;;
    boot|bootloader)
        flash_bootloader
        ;;
    a|slot_a)
        flash_a
        ;;
    b|slot_b)
        flash_b
        ;;
    apps)
        flash_a
        flash_b
        ;;
    *)
        echo "usage: $0 [all|metadata|boot|a|b|apps]   (default: all)"
        exit 1
        ;;
esac

echo "✅ All done! Please reset the board."
