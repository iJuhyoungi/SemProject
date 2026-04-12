#ifndef RT1020_REGS_H
#define RT1020_REGS_H

#include <stdint.h>

/* [CCM] 클럭 제어 모듈 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
#define CCM_CSCDR1                              (*(volatile uint32_t *)0x400FC024)
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)
/* [CCM] 클럭 제어 모듈 추가 */
#define CCM_CCGR3                               (*(volatile uint32_t *)0x400FC074) // SEMC 모듈 전원/클럭 활성화
#define CCM_CCGR5                               (*(volatile uint32_t *)0x400FC07C)

/* [IOMUXC] 핀 다중화 모듈 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0) // LED
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06     (*(volatile uint32_t *)0x401F80D4) // UART1 TX
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07     (*(volatile uint32_t *)0x401F80D8) // UART1 RX

/* [GPIO1] 범용 입출력 모듈 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)

/* [LPUART1] 시리얼 통신 모듈 */
#define LPUART1_BAUD                            (*(volatile uint32_t *)0x40184010)
#define LPUART1_STAT                            (*(volatile uint32_t *)0x40184014)
#define LPUART1_CTRL                            (*(volatile uint32_t *)0x40184018)
#define LPUART1_DATA                            (*(volatile uint32_t *)0x4018401C)

/* 🚀 [SEMC] Smart External Memory Controller */
#define SEMC_MCR                                (*(volatile uint32_t *)0x402F0000) // 모듈 제어
#define SEMC_INTR                               (*(volatile uint32_t *)0x402F003C) // 인터럽트 상태 플래그
#define SEMC_BR0                                (*(volatile uint32_t *)0x402F0010) // Base Register 0 (주소 및 크기)

#define SEMC_SDRAMCR0                           (*(volatile uint32_t *)0x402F0040) // SDRAM 설정 (CAS, Burst 등)
#define SEMC_SDRAMCR1                           (*(volatile uint32_t *)0x402F0044) // SDRAM 타이밍 1 (tRCD, tRP 등)
#define SEMC_SDRAMCR2                           (*(volatile uint32_t *)0x402F0048) // SDRAM 타이밍 2
#define SEMC_SDRAMCR3                           (*(volatile uint32_t *)0x402F004C) // SDRAM 타이밍 3 (Refresh)

#define SEMC_IPTXDAT                            (*(volatile uint32_t *)0x402F00A0) // IP Command 전송 시 데이터 전달 레지스터

/* 🚀 IP Command (수동으로 SDRAM에 마법의 주문을 쏘는 창구) */
#define SEMC_IPCR0                              (*(volatile uint32_t *)0x402F0090) // 명령어 인자 (주소 등)
#define SEMC_IPCR1                              (*(volatile uint32_t *)0x402F0094) // 명령어 인자 (데이터 사이즈)
#define SEMC_IPCR2                              (*(volatile uint32_t *)0x402F0098) // 명령어 인자
#define SEMC_IPCMD                              (*(volatile uint32_t *)0x402F009C) // 실제 명령어 트리거 레지스터

/* 🚀 [NVIC] Nested Vectored Interrupt Controller */
#define NVIC_ISER0                              (*(volatile uint32_t *)0xE000E100) // 인터럽트 허용 레지스터 0



/* [IOMUXC] SEMC 핀 MUX 시작 주소 */

/*
 **Chapter 27 (SEMC)**를 보고: "아, 이 칩에서 SDRAM을 돌리려면 SEMC라는 엔진을 써야 하는구나."
 **Chapter 10 (Pin Muxing)**을 보고: "SEMC 엔진의 신호를 밖으로 내보내려면 GPIO_EMC_xx 핀들을 써야 하고, 이때 ALT0 모드를 골라야 하는구나."
 **Chapter 11 (IOMUXC)**를 보고: "그러므로 IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_xx 레지스터 값을 0으로 세팅하면 물리적으로 SDRAM 컨트롤러와 핀이 연결되겠구나!"
 */

#define IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00       (0x401F8014)

#endif // RT1020_REGS_H
