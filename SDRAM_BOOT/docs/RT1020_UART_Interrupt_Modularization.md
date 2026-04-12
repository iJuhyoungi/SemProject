# NXP i.MX RT1020 베어메탈 모듈화 및 UART RX 인터럽트 구현

## 1. 프로젝트 개요
본 프로젝트는 132MHz로 부스팅된 베어메탈 환경에서, 거대해진 `main.c`를 기능별로 모듈화(Decoupling)하고 PC와의 양방향 통신(UART TX/RX)을 구현한 과정입니다.
특히 Polling 방식이 아닌 **하드웨어 인터럽트(Interrupt) 방식**을 사용하여, CPU가 대기 상태(WFI)에 있다가 키보드 입력이 들어오는 즉시 깨어나 LED를 제어하도록 설계되었습니다.

---

## 2. 핵심 아키텍처: 데이터시트의 비밀 (NXP vs Arm)

MCU의 데이터시트(`NXP_RT1020_RM.pdf`)에 `NVIC`나 예외 처리 벡터 테이블의 주소가 없는 이유는, 해당 하드웨어가 NXP가 아닌 **Arm Cortex-M7 코어의 고유 영역**이기 때문입니다.

### 🏛️ 인터럽트 벡터 테이블 (`__VECTOR_TABLE`) 매핑 공식
* **0번 ~ 15번:** Arm 아키텍처의 절대 법칙. 전 세계 모든 Cortex-M 코어가 동일하게 사용함 (`Reset`, `NMI`, `HardFault`, `SysTick` 등).
* **16번 이후:** 칩 제조사(NXP)의 외부 모듈 인터럽트 연결 공간.
* **배열 인덱스 계산법:** `배열 자리 = IRQ 번호 + 16`
    * `LPUART1`의 IRQ 번호는 20번이므로, 배열의 **36번째 자리(16 + 20)**에 핸들러 함수를 매핑해야 함.

### ⚡ NVIC (Nested Vectored Interrupt Controller)
* Arm 코어에 내장된 인터럽트 통제 센터 (Base Address: `0xE000E000` 대역).
* `NVIC_ISER0` (`0xE000E100`): IRQ 0번부터 31번까지의 인터럽트를 활성화하는 레지스터.

---

## 3. 하드웨어 레지스터 맵 (`rt1020_regs.h`)
모든 모듈에서 공통으로 참조하는 절대 주소 매크로. (NVIC 코어 레지스터 추가)

```c
#ifndef RT1020_REGS_H
#define RT1020_REGS_H

#include <stdint.h>

/* [CCM] 클럭 제어 모듈 */
// ... (생략: 기존 클럭 매크로) ...

/* [IOMUXC] 핀 다중화 모듈 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0) // LED
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06     (*(volatile uint32_t *)0x401F80D4) // LPUART1 TX
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07     (*(volatile uint32_t *)0x401F80D8) // LPUART1 RX

/* [GPIO1] 범용 입출력 모듈 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)

/* [LPUART1] 시리얼 통신 모듈 */
#define LPUART1_BAUD                            (*(volatile uint32_t *)0x40184010)
#define LPUART1_STAT                            (*(volatile uint32_t *)0x40184014)
#define LPUART1_CTRL                            (*(volatile uint32_t *)0x40184018)
#define LPUART1_DATA                            (*(volatile uint32_t *)0x4018401C)

/* 🚀 [NVIC] 인터럽트 컨트롤러 (Arm Core) */
#define NVIC_ISER0                              (*(volatile uint32_t *)0xE000E100)

#endif // RT1020_REGS_H
