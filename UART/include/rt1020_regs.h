#ifndef RT1020_REGS_H
#define RT1020_REGS_H

#include <stdint.h>

/* [CCM] 클럭 제어 모듈 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
#define CCM_CSCDR1                              (*(volatile uint32_t *)0x400FC024)
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)
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

/* 🚀 [NVIC] Nested Vectored Interrupt Controller */
#define NVIC_ISER0                              (*(volatile uint32_t *)0xE000E100) // 인터럽트 허용 레지스터 0

#endif // RT1020_REGS_H
