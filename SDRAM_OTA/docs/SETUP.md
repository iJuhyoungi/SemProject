# 📅 DevLog: 2026-03-27 (금)

## 🎯 Objective
- i.MX RT1020 (Cortex-M7) 베어메탈 환경 구축 및 CMake 빌드 시스템 세팅
- 링커 스크립트, 스타트업 코드, 그리고 NXP 특유의 부트 시퀀스(FCB, IVT) 이해 및 적용

## 🚨 Issues Encountered
1. **CMake 컴파일러 테스트 에러 (`undefined reference to _exit, _read...`)**
   - 베어메탈 환경이라 C 표준 라이브러리의 시스템 콜이 없어서 CMake가 컴파일러가 고장 났다고 오판함.
2. **GCC `[-Wpedantic]` ISO C 표준 경고 및 링커 에러**
   - 배열 범위 지정(`[16 ... 175]`) 및 함수 포인터 캐스팅 문제로 표준 문법 경고 발생.
   - NXP CMSIS 내부 함수(`SystemInit`)가 `__VECTOR_TABLE`을 찾지 못해 링커 에러 발생.
3. **부팅을 위한 NXP 고유 데이터 누락**
   - Boot ROM이 외부 Flash를 읽기 위해 필요한 FCB(Flash Configuration Block)와 IVT(Image Vector Table)가 없었음. (이대로 보드에 올렸다면 먹통이 되었을 것)

## 💡 Solutions & Key Learnings

### 1. CMake 빌드 시스템 우회
- `CMakeLists.txt` 최상단에 `set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")`를 추가하여, 실행 파일(.elf) 대신 정적 라이브러리로 컴파일러 테스트를 진행하도록 우회.
- 링커 플래그에 `--specs=nosys.specs`를 추가하여 가짜 시스템 콜(Stub)을 제공.

### 2. Cortex-M7 & i.MX RT1020 필수 초기화 (스타트업 코드)
- **VTOR (Vector Table Offset Register) 재배치:** XIP(Flash 실행) 환경이므로, 기본값(`0x00000000`)이 아닌 우리가 링킹한 Flash 주소로 인터럽트 벡터 테이블 위치를 명시적으로 알려주어야 함 (`SCB_VTOR = (uint32_t)__VECTOR_TABLE;`).
- **FPU (Floating Point Unit) 활성화:** Cortex-M7의 하드웨어 FPU를 부팅 초기에 켜주어야 실수 연산 시 HardFault가 발생하지 않음 (`SCB_CPACR |= (0xF << 20);`).
- **메모리 복사 (LMA vs VMA):** `.data` 섹션을 Flash(`LOADADDR`)에서 최고속 RAM인 `DTCM`으로 복사하는 로직 구현.

### 3. NXP Boot Sequence (매우 중요)
- 일반적인 MCU와 달리, i.MX RT 시리즈는 내부 Boot ROM이 외부 Flash 메모리를 먼저 스캔함.
- **FCB (0x60000000):** Flash 메모리의 스펙(QSPI, 명령어 패드 설정 등)을 Boot ROM에 알려주는 512바이트 크기의 설정표. 바이트 오프셋(Byte-offset)이 1바이트라도 어긋나면 부팅 실패.
- **IVT (0x60001000):** Boot ROM이 FCB를 읽은 후, 실제 펌웨어의 진입점(`Reset_Handler`)을 찾기 위한 이정표.
- 이 두 가지를 C 구조체로 완벽히 구현하고 `__attribute__((section(...)))`을 이용해 링커 스크립트가 지정한 절대 주소에 정확히 꽂아 넣음.

### 4. ISO C (C99) 표준 준수
- GCC 확장 문법 대신, `union`을 사용하여 데이터 포인터(스택 주소)와 함수 포인터(인터럽트 핸들러)를 안전하게 담는 배열을 구현.
- 매크로를 활용해 160개의 인터럽트를 하드코딩 없이 깔끔하게 채움.

## 📝 Code Snippets (Key Changes)
*전체 코드는 별도 파일로 보관 (`flexspi_nor_config.c`, `boot_header.c`, `startup_MIMXRT1020.c`, `MIMXRT1020_flexspi.ld`)*

```cmake
# CMakeLists.txt 핵심 수정부
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
# ... 생략 ...
set(COMMON_FLAGS "${MCPU_FLAGS} --specs=nosys.specs -ffreestanding ...")
