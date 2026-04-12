# NXP i.MX RT1020 Bare-Metal Firmware Project

## 📌 Project Overview
본 프로젝트는 NXP Cortex-M7 기반의 i.MX RT1020 MCU를 사용하여, 엔터프라이즈급 반도체(SSD) 펌웨어 수준의 고속 데이터 처리 및 메모리 제어를 목표로 하는 베어메탈(Bare-Metal) 펌웨어 개발기입니다. 
HAL 라이브러리를 배제하고 레지스터(Register) 레벨의 직접 제어를 통해 하드웨어 아키텍처에 대한 깊은 이해와 최적화를 달성했습니다.

---

## 🚀 Phase 1: Core Architecture & Memory Initialization

### 1.1 System Clock & Basic I/O 
* **목표:** MCU의 코어 클럭을 타겟 속도로 펌핑하고 기본적인 디버깅 환경 구축.
* **구현 내용:** CPU 클럭을 528MHz Sys PLL 소스로부터 분주하여 안정적인 132MHz로 스위칭 성공 및 LPUART1 기반 폴링(Polling) 디버그 터미널 구축.

### 1.2 SDRAM Controller (SEMC) 정밀 제어 및 최적화
* **목표:** 32MB 외부 SDRAM (IS42S16160J) 연동 및 132MHz 고속 데이터 버스 무결성 확보.
* **트러블슈팅 (1-Cycle Shift 버그):**
  * SDRAM 읽기/쓰기 테스트 중 데이터가 1 클럭씩 밀리는 치명적 타이밍 이슈 발생.
  * SEMC 컨트롤러의 기대 CAS Latency(CL)와 실제 SDRAM 디바이스에 전송된 Mode Register Set(MRS) 값의 불일치를 원인으로 규명.
  * `SEMC_SDRAMCR0` 설정값(CL=3)과 SDRAM 데이터시트의 MRS(0x33)를 비트 단위로 완벽하게 일치시키고, DQS 루프백 핀(SION)을 개방하여 16-bit / 32-bit 패킹 가혹 테스트 ALL PASS 달성.

---

## 🧠 Deep Dive: XIP to SDRAM Boot Architecture (Phase 1.3)

본 프로젝트의 핵심은 느린 NOR Flash 실행 환경(XIP, eXecute In Place)을 탈피하여, 펌웨어 스스로 자신을 고속 SDRAM으로 복사하고 실행 위치를 점프하는 **자체 릴로케이션(Relocation) 부트로더**를 구현한 것입니다. 이를 위해 링커 스크립트와 스타트업 코드를 전면 재설계했습니다.

### 1. 링커 스크립트 (.ld) 정밀 수술 (VMA / LMA 분리)
일반적인 환경에서는 코드가 저장된 위치(LMA, Load Memory Address)와 실행되는 위치(VMA, Virtual Memory Address)가 동일합니다. 본 프로젝트에서는 컴파일러의 메모리 맵핑을 의도적으로 분리하여 SDRAM 실행 환경을 구축했습니다.

* **SDRAM 영토 선포 및 메인 코드 분리:**
  메인 로직(`.text`), 초기화된 변수(`.data`), 0 초기화 변수(`.bss`), 힙(`.heap`)을 모두 `0x80000000`(SDRAM) 영역으로 할당했습니다. 
  ```ld
  .text :
  {
      __text_start__ = .;
      *(.text*)
      __text_end__ = .;
  } > SDRAM AT> FLASH  /* 핵심: 저장은 FLASH에, 실행은 SDRAM에서! */
  
  __text_load_start__ = LOADADDR(.text); /* FLASH에 구워진 원본 주소를 기억 */
  ```

* **부트로더 잔류 부대(`.boot_text`) 고정:**
  SDRAM 전원이 켜지기 전에 불리는 함수들(Clock, UART, SDRAM Init)이 SDRAM 영역으로 맵핑되면 하드폴트가 발생합니다. 이를 방지하기 위해 해당 모듈과 문자열 상수(`.rodata`)를 안전한 플래시 영역에 강제 고정했습니다.
  ```ld
  .boot_text :
  {
      *(.text.Reset_Handler)
      *(.text.SystemInit)
      *(.rodata*) /* 문자열 상수도 플래시에 고정하여 BusFault 방지 */
  } > FLASH
  ```

### 2. Startup 코드 (startup.c) 기반 부트로더 구현
`Reset_Handler`는 칩에 전원이 인가되었을 때 가장 먼저 실행되는 함수로, 본 프로젝트에서는 이 함수를 단순한 초기화가 아닌 **메모리 복사 부트로더**로 격상시켰습니다.

* **1단계: 생명줄 연결 및 SDRAM 기상**
  하드웨어 워치독(Watchdog)을 해제하고, 코어 클럭(132MHz) 및 디버그용 UART를 활성화한 뒤 SDRAM 컨트롤러를 초기화합니다.
* **2단계: Firmware Relocation (플래시 ➔ 램 복사)**
  링커 스크립트가 만들어둔 기호(`__text_load_start__`, `__text_start__` 등)를 활용하여 플래시에 구워진 바이너리를 SDRAM으로 복사합니다. 이때 컴파일러의 `memcpy` 최적화(표준 라이브러리가 아직 없는 SDRAM을 참조하는 오류)를 막기 위해 포인터에 `volatile` 속성을 부여했습니다.
  ```c
  volatile uint32_t *src = (volatile uint32_t *)&__text_load_start__;
  volatile uint32_t *dst = (volatile uint32_t *)&__text_start__;
  while (dst < (volatile uint32_t *)&__text_end__) {
      *dst++ = *src++;
  }
  ```
* **3단계: Vector Table 점프 및 `main()` 진입**
  인터럽트 예외 주소록인 VTOR(`SCB->VTOR`)을 SDRAM에 복사된 벡터 테이블 주소로 덮어씌웁니다. 이후 `main()` 함수를 호출하면, 코어는 자신이 0x80000000(SDRAM)에서 동작하고 있음을 인식하고 초고속으로 명령어를 수행하기 시작합니다.

### 💡 성과 및 시사점
이러한 XIP to RAM 부트로더 아키텍처는 엔터프라이즈급 반도체 펌웨어에서 필수적으로 요구되는 기법입니다. 이를 통해 칩 내부 플래시의 속도 한계와 Jitter(지연)를 극복하고, 이후 추가될 DMA/SPI/ADC 등 대용량 데이터 처리 모듈을 병목 없이 수용할 수 있는 완벽한 소프트웨어 기반을 다졌습니다.
