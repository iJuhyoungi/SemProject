# i.MX RT1020 Bare-Metal Project

이 프로젝트는 i.MX RT1020(Cortex-M7) MCU를 대상으로 하는 bare-metal 템플릿입니다.  
목표는 벤더 HAL이나 MCUXpresso SDK 없이, 레지스터 수준에서 직접 MCU 기능을 추가하는 기반을 제공하는 것입니다.

현재 소스 트리는 다음과 같은 벤더 계층에 의존하지 않습니다.

- `fsl_*` 드라이버
- `BOARD_*`, `CLOCK_*` 초기화 래퍼
- MCUXpresso SDK 프로젝트 생성물

대신 외부 FlexSPI NOR Flash에서 XIP(Execute In Place)로 부팅하는 구성을 전제로, 스타트업 코드, 벡터 테이블, GNU ld 링커 스크립트, 기본 부트 헤더(IVT/Boot Data/FlexSPI Config 템플릿)만 포함합니다.

## 대상

- MCU: i.MX RT1020
- Core: Arm Cortex-M7
- 개발 방향: HAL 비의존 bare-metal
- 부트 방식: External FlexSPI NOR XIP
- 기본 메모리 가정:
  - `ITCM`: `0x00000000`, 64 KB
  - `DTCM`: `0x20000000`, 64 KB
  - `OCRAM`: `0x20200000`, 128 KB
- 플래시 이미지 배치:
  - FlexSPI config block: `0x60000000`
  - IVT / Boot Data: `0x60001000`
  - Application image (`.text`): `0x60002000` 이후

## 목표

- 벤더 HAL 없이 bare-metal로 프로젝트를 유지
- 스타트업, 인터럽트 벡터, 링커 스크립트를 직접 관리
- 필요한 주변장치는 레지스터 정의와 드라이버를 직접 추가

## 비목표

- MCUXpresso SDK 구조에 맞춘 코드베이스 유지
- 자동 생성 코드에 의존하는 초기화 흐름
- 특정 EVK BSP에 종속되는 추상화 계층 제공

## 프로젝트 구조

- `src/`
  - `main.c`: 최소 애플리케이션 엔트리
  - `system_MIMXRT1020.c`: 시스템 초기화와 `SystemCoreClock`
  - `boot_header.c`: FlexSPI config / IVT / Boot Data 템플릿
- `include/`
  - `mimxrt1020.h`: 최소 코어 레지스터 정의
  - `system_MIMXRT1020.h`: 시스템 초기화 헤더
- `startup/`
  - `startup_MIMXRT1020.c`: 벡터 테이블, `Reset_Handler`, 기본 예외 핸들러
- `linker/`
  - `MIMXRT1020_flexspi.ld`: FlexSPI XIP 기준 GNU ld 스크립트
- `docs/`
  - `README.md`: 골격 설명
  - `MEMORY_MAP.md`: 메모리 배치 메모

## 전제 조건

다음 툴이 필요합니다.

- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `arm-none-eabi-size`
- `make` 또는 `cmake`

Ubuntu 계열 예시:

```sh
sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi make cmake
```

## 빌드

### Make 사용

```sh
make
```

생성물:

- `build/mimxrt1020_baremetal.elf`
- `build/mimxrt1020_baremetal.bin`
- `build/mimxrt1020_baremetal.map`

정리:

```sh
make clean
```

### CMake 사용

```sh
cmake -S . -B build-cmake
cmake --build build-cmake
```

## 부트 및 플래시 전 주의 사항

현재 프로젝트의 [`src/boot_header.c`](/home/juhyoung/MCU/src/boot_header.c)는 바로 양산 보드에 쓰는 최종 설정이 아닙니다.

- FlexSPI NOR 설정 블록은 템플릿입니다.
- 실제 보드에 실장된 NOR Flash 파트 번호에 맞게 LUT, timing, flash size를 수정해야 합니다.
- 클럭, pin mux, MPU/cache 정책, 주변장치 초기화는 포함되어 있지 않습니다.
- 보드가 EVK가 아니거나 부트 모드 설정이 다르면 플래시 후 동작이 달라질 수 있습니다.

즉, `bin` 파일은 생성할 수 있어도 실제 ROM 부트 성공 여부는 외부 flash 설정값이 맞아야 결정됩니다.

## Flash 방법

RT1020은 일반적인 내부 flash MCU와 달리 보통 외부 FlexSPI NOR에 이미지를 기록합니다.  
아래 예시는 대표적인 흐름이며, 실제 사용 툴은 디버거 종류와 보드 지원 패키지에 따라 달라질 수 있습니다.

### 1. J-Link 사용

J-Link가 RT1020 대상과 외부 flash loader를 지원하는 환경이면 보통 ELF 또는 BIN을 직접 기록할 수 있습니다.

예시:

```sh
JLinkExe
```

J-Link 콘솔에서 예시 흐름:

```text
device MIMXRT1021xxx5A
si SWD
speed 4000
connect
loadfile build/mimxrt1020_baremetal.elf
r
g
q
```

주의:

- 장치명은 실제 사용 중인 RT1020 파생형에 맞춰야 합니다.
- 외부 flash loader가 올바르게 잡혀야 `0x60000000` 영역 프로그래밍이 됩니다.

### 2. LinkServer / IDE 사용

IDE 또는 LinkServer 환경에서는 보드/디버거 설정에 맞는 flash driver가 있으면 ELF를 그대로 다운로드할 수 있습니다.

일반 절차:

1. 프로젝트 ELF 생성
2. 디버거 대상 장치를 `MIMXRT1021xxxxx` 계열로 선택
3. 외부 FlexSPI flash driver 또는 보드 지원 설정 적용
4. Program / Debug 실행

만약 `No flash configured at the specified address` 같은 오류가 나오면:

- 잘못된 flash driver 선택
- 부트 헤더/flash config mismatch
- 보드의 외부 flash 설정 불일치

중 하나일 가능성이 큽니다.

### 3. ROM 다운로드 유틸리티 또는 제조 툴 사용

ROM 다운로드 유틸리티나 제조 툴을 써서 `.bin`을 외부 flash에 써 넣을 수도 있습니다.

일반 절차:

1. MCU를 Serial Downloader 모드로 진입
2. PC와 USB/UART로 연결
3. 대상 디바이스 선택
4. `build/mimxrt1020_baremetal.bin` 지정
5. 시작 주소를 외부 FlexSPI XIP base인 `0x60000000`로 설정
6. Program 실행

주의:

- 이 방식도 ROM이 이해할 수 있는 유효한 flash config / IVT 구조가 있어야 정상 부팅됩니다.
- 일부 툴은 `.bin` 외에 `.elf` 또는 SB 파일 기반 흐름을 사용할 수 있습니다.

## 디버깅 팁

- 부팅 직후 멈추면 먼저 `0x60000000`의 flash config block이 맞는지 확인합니다.
- `Reset_Handler` 진입 여부를 확인합니다.
- `SCB->VTOR`가 벡터 테이블 주소로 설정되었는지 확인합니다.
- `.data` 복사와 `.bss` 초기화가 정상인지 확인합니다.
- 부트 모드 핀과 전원 시퀀스, 디버거 reset strategy도 점검합니다.

## 다음 작업 권장 사항

실사용 프로젝트로 확장하려면 보통 아래를 추가합니다.

- 보드별 `board.c` / `board.h`
- clock init
- IOMUXC pin mux 설정
- SysTick 또는 GPT 기반 timebase
- UART debug console
- cache/MPU 초기화
- 외부 NOR flash에 맞는 정확한 FlexSPI LUT
- 필요한 주변장치 레지스터 맵과 bare-metal 드라이버

## 관련 파일

- [`src/boot_header.c`](/home/juhyoung/MCU/src/boot_header.c)
- [`startup/startup_MIMXRT1020.c`](/home/juhyoung/MCU/startup/startup_MIMXRT1020.c)
- [`linker/MIMXRT1020_flexspi.ld`](/home/juhyoung/MCU/linker/MIMXRT1020_flexspi.ld)
- [`docs/README.md`](/home/juhyoung/MCU/docs/README.md)
- [`docs/MEMORY_MAP.md`](/home/juhyoung/MCU/docs/MEMORY_MAP.md)
