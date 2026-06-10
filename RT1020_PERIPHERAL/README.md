# LPSPI Bare-metal Driver — i.MX RT1020

> HAL / SDK / 외부 라이브러리 없이 Reference Manual 만 보고 i.MX RT1020 (Cortex-M7) 의
> LPSPI 페리페럴을 **register-level 로 직접** 올린 베어메탈 프로젝트입니다.
> ROM 부팅 체인부터 시작해 CCM 클럭 게이팅 → IOMUXC 핀 muxing → LPSPI 마스터 init →
> polled 송신까지, 각 단계를 보드에서 실물 검증하며 쌓아 올렸습니다.

[![target](https://img.shields.io/badge/target-i.MX_RT1020-blue)]()
[![arch](https://img.shields.io/badge/arch-Cortex--M7-blue)]()
[![peripheral](https://img.shields.io/badge/peripheral-LPSPI1_master-orange)]()
[![deps](https://img.shields.io/badge/HAL%2FSDK_deps-0-success)]()
[![milestone](https://img.shields.io/badge/milestone-P--1_complete-blueviolet)]()

---

## 한눈에

AUTOSAR set 개발 본업의 **SPI Handler / SPI Driver layer** 를 베어메탈에서 register-level 로
재구현하는 것이 목표입니다. 추상화 계층 없이 RM 의 레지스터 시퀀스를 직접 다뤄,
"1 byte 가 핀으로 나가기까지 무엇이 어떤 순서로 일어나는가" 를 끝까지 따라갑니다.

주요 특징입니다.

- **HAL / SDK 를 쓰지 않습니다.** CCM, IOMUXC, LPSPI 레지스터를 RM 보고 직접 세팅합니다.
- **부팅 체인을 직접 세웁니다.** FCB / IVT 부터 `Reset_Handler → main` 까지 (`ROM → 0x60001000 IVT → entry`).
- **LPSPI 마스터 bring-up 시퀀스를 register-level 로 구현합니다.**
  `CCM clock → IOMUXC pin → CR[RST] → CFGR1[MASTER] → CCR[SCKDIV] → FCR → CR[MEN] → TCR/TDR 송신`.
- **단계마다 실물로 검증합니다.** VERID / CFGR1 / CCR / SR readback 과 `SR[TCF]` (전송 완료 플래그) 로
  각 단계가 칩에서 실제로 동작했음을 UART 로 확인합니다.
- **shared/ plumbing 을 재사용합니다.** startup / clock / uart / led 등은 SDRAM_SECURE_BOOT 에서 검증된 공통 코드.

---

## 시작하기

```bash
./build.sh        # arm-none-eabi-gcc 로 단일 이미지 빌드 (build/rt1020_peripheral.{elf,bin})
./flash_mcu.sh    # pyocd 로 0x60000000 에 flash (-t mimxrt1020)

# 시리얼 모니터 (별도 터미널)
gtkterm -p /dev/ttyACM0 -s 115200
# 보드 SW7 reset → 부팅 + LPSPI bring-up 로그 + LED blink 확인
```

기대 출력 (요약):

```
[PERI] hello from main()
[PERI] LPSPI1 pins muxed (ALT1)
[PERI] LPSPI1 CFGR1 = 0x00000001     # MASTER
[PERI] LPSPI1 CCR   = 0x00000082     # SCKDIV=130, SCK≈1MHz
[PERI] LPSPI1 SR    = 0x00000001     # TDF
[PERI] TX 0xA5 ...
[PERI] TX OK (TCF set)               # 송신 엔진 동작 검증
```

---

## 문서

- **[docs/LPSPI_NOTES.md](docs/LPSPI_NOTES.md)** — RM Chapter 43(LPSPI) / 14(CCM) / 11(IOMUXC) 정리.
  레지스터 메모리 맵, 비트 필드, 클럭 트리, 핀 매핑, bring-up 시퀀스, 단계별 검증 기준.
  "코드 짜기 전 지도" 로서 P-1a 에서 작성 후 각 단계마다 갱신.

---

## 프로젝트 구조

```
RT1020_PERIPHERAL/
├── CMakeLists.txt              # 단일 이미지 빌드 (IS_BOOTLOADER 분기 = flash XIP)
├── arm-none-eabi-toolchain.cmake
├── build.sh / flash_mcu.sh
├── linker/peripheral.ld        # 단일 이미지 메모리 맵 (FCB/IVT/FLASH/DTCM/ITCM)
├── src/
│   ├── boot_header.c           # IVT + boot_data (ROM 이 읽는 부팅 헤더)
│   └── main.c                  # LPSPI1 clock/pin/init/send + bring-up 로그
├── shared/                     # 검증된 공통 plumbing (startup, clock, uart, led, regs...)
└── docs/LPSPI_NOTES.md         # RM 정리 노트
```

---

## 진행 상황

### 완료된 항목

- **P-0 부팅 체인** — `boot_header.c`(IVT) + `peripheral.ld`(단일 이미지 맵) + `CMakeLists.txt`.
  ROM → FCB(0x60000000) → IVT(0x60001000) → `Reset_Handler` → `main`. UART hello + LED blink 실물 확인.
- **P-1 LPSPI1 마스터 bring-up (register-level)**
  - P-1a: RM Chapter 43 정리 → `docs/LPSPI_NOTES.md`
  - P-1b: CCM 클럭 — `CCGR1[CG0]` gate ON, root PLL2/4 = 132MHz. VERID read 로 버스 생존 검증
  - P-1c: IOMUXC — LPSPI1 `SCK/PCS0/SDO/SDI` → `GPIO_AD_B0_10~13` ALT1
  - P-1d: init 시퀀스 — `CR[RST] → CFGR1[MASTER] → CCR[SCKDIV] → FCR → CR[MEN]`, readback 검증
  - P-1e: TX-only 송신 — `TCR`(8bit, RXMSK) + `TDR`, `SR[TCF]` / `FSR` polled 검증

### 다음 단계 (미정 — 방향 고민 중)

- **P-1f** — 외부 SPI device 연결 후 RX 까지 (read ID 등 진짜 송수신). *device + 헤더 납땜 필요.*
- **P-2** — TX/RX FIFO burst (watermark 활용). *부품 불요.*
- **P-3** — IRQ 기반 송수신 + cycle counter. *부품 불요. AUTOSAR 인터럽트 기반 SPI Handler narrative 에 직결.*

---

## 주의 / 현재 범위의 한계

- **TX-only 검증입니다.** MIMXRT1020-EVK 의 Arduino 헤더(LPSPI1 이 나오는 J19)가 unpopulated 라
  loopback 점퍼를 바로 못 꽂습니다. 그래서 P-1e 는 송신 경로를 `SR[TCF]` / `FSR` 로 검증하고,
  RX / loopback 은 device 연결(P-1f) 시점으로 미뤘습니다. (J19: D11=SDO, D12=SDI)
- **클럭은 ROM 기본값을 사용합니다.** startup 의 `IS_BOOTLOADER` 분기(flash XIP)를 재사용하므로
  `Clock_Init_132MHz()` 를 호출하지 않습니다. LPSPI 기능 클럭은 ROM 기본 PLL2/4 = 132MHz.
- **`shared/` 에 secure-boot 잔재가 포함됩니다.** CMake 의 `file(GLOB)` 가 전부 컴파일하지만
  `--gc-sections` 로 미사용 코드(RSA/SHA 등)는 최종 이미지에서 제거됩니다.
