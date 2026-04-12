#!/bin/bash

echo "========================================"
echo " 🧹 1. Erasing entire Flash Memory..."
echo "========================================"
# 기존처럼 칩 전체를 한 번 시원하게 밀어줍니다. (쓰레기 데이터 충돌 방지)
pyocd erase -t mimxrt1021dag5a --chip


echo ""
echo "========================================"
echo " 🛡️ 2. Flashing BOOTLOADER (0x60000000)"
echo "========================================"
# 부트로더 바이너리를 플래시의 가장 첫 번째 섹터(0x60000000)에 굽습니다.
pyocd flash build/bootloader/bootloader.bin -t mimxrt1021dag5a --base-address 0x60000000


echo ""
echo "========================================"
echo " 🚀 3. Flashing MAIN APP (0x60040000)"
echo "========================================"
# 메인 앱(Slot A) 바이너리를 부트로더 영역이 끝난 지점(0x60040000)에 굽습니다.
pyocd flash build/app/app.bin -t mimxrt1021dag5a --base-address 0x60040000

echo ""
echo "✅ All done! Please reset the board."
