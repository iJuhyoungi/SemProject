#!/bin/bash

# Usage: ./flash_mcu.sh [target]
#   (no arg)   = all (default)                                                                                                                                                           
#   all        = chip erase + flash 6 images (stage1 + bootloader + recovery + metadata + a + b)                                                                                         
#   stage1     = bootloader_stage1 만                                                                                                                                                    
#   boot       = stage 2 bootloader 만                                                                                                                                                   
#   recovery   = recovery slot 만                                    ← NEW                                                                                                               
#   metadata   = boot_ctrl 만                                                                                                                                                            
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

flash_recovery() {                                                                                                                                                                       
    echo "========================================"       
    echo " Flashing RECOVERY (0x603E0000)"
    echo "========================================"                                                                                                                                      
    # Recovery slot. Stage 1 이 Stage 2 검증 실패 시 점프하는 fallback target.
    # 64KB 영역. 현재 passive (UART 알림 + LED 깜빡), Phase 4 Step 4 에서 active 로 확장 예정.                                                                                           
    pyocd flash build/bootloader/recovery/bootloader_recovery.bin $DEV --base-address 0x603E0000                                                                                         
    echo ""                                                                                                                                                                              
}

flash_stage1() {
    echo "========================================"
    echo " Flashing Stage 1 (0x60000000)"
    echo "========================================"
    # Stage 1 immutabgle. FCB + IVT + 작은 jump 코드. 0x60000000 부터 8KB 차지
    pyocd flash build/bootloader/stage1/bootloader_stage1.bin $DEV --base-address 0x60000000
    echo ""
}

flash_bootloader() {
    echo "========================================"
    echo " Flashing BOOTLOADER (0x60004000)"
    echo "========================================"
    # bootloader는 Stage 2가 되어 0x60004000 부터 시작. Stage 1에서 jump 하도록 되어 있음.
    # FCB/IVT 는 Stage1에서 담당하므로 bootloader는 순수 실행 코드만 포함. 0x60004000 부터 120KB 차지
    pyocd flash build/bootloader/stage2/bootloader_stage2.bin $DEV --base-address 0x60004000
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
        flash_stage1
        flash_bootloader
        flash_recovery
        flash_metadata
        flash_a
        flash_b
        ;;
    recovery)
        flash_recovery
        ;;
    stage1)
        flash_stage1
        ;;
    metadata)
        flash_metadata
        ;;
    boot|bootloader|stage2)
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
        echo "usage: $0 [all|stage1|recovery|metadata|boot|a|b|apps]   (default: all)"
        exit 1
        ;;
esac

echo "✅ All done! Please reset the board."
