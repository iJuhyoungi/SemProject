#!/bin/bash

echo "========================================"
echo " 1. Erasing entire Flash Memory..."
echo "========================================"
pyocd erase -t mimxrt1021dag5a --chip

echo ""
echo "========================================"
echo " 2. Flashing BOOTLOADER ELF"
echo "========================================"
pyocd flash build/bootloader/bootloader.elf -t mimxrt1021dag5a

echo ""
echo "========================================"
echo " 3. Flashing BOOT CTRL"
echo "========================================"
pyocd flash build/bootctrl/boot_ctrl.bin -t mimxrt1021dag5a --base-address 0x603F0000

echo ""
echo "========================================"
echo " 4. Flashing APP SLOT A (0x60040000)"
echo "========================================"
pyocd flash build/app_slot_a/app_slot_a.bin -t mimxrt1021dag5a --base-address 0x60040000

echo ""
echo "========================================"
echo " 5. Flashing APP SLOT B (0x60200000)"
echo "========================================"
pyocd flash build/app_slot_b/app_slot_b.bin -t mimxrt1021dag5a --base-address 0x60200000

echo ""
echo "✅ All done! Please reset the board."