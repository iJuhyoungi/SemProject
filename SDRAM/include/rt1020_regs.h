#ifndef RT1020_REGS_H
#define RT1020_REGS_H

#include <stdint.h>

/*
 * rt1020_regs.h
 * -----------------------------------------------------------------------------
 * i.MX RT1020 의 하드웨어 레지스터 절대 주소 매핑 테이블.
 * 각 매크로는 해당 주소를 volatile 포인터로 역참조하므로, 일반 변수처럼
 * 읽고 쓰면 곧바로 하드웨어 레지스터에 접근한다. (예: CCM_CCGR3 |= ...)
 * 예외는 파일 맨 아래 IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00 — 주석 참고.
 * -----------------------------------------------------------------------------
 */

/* [CCM] Clock Controller Module — 칩 전체 클럭의 소스/분주/게이팅 제어 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014) // Bus Clock Divider — AHB 분주, periph 경로 선택
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018) // Bus Clock Mux — 시스템 클럭 소스(PLL) 선택
#define CCM_CSCDR1                              (*(volatile uint32_t *)0x400FC024) // Serial Clock Divider 1 — UART 등 클럭 소스/분주
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048) // Divider Handshake In-Process — 분주 변경 완료 상태(BUSY)
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C) // Clock Gating Reg 1 — GPIO1 등 모듈 클럭 ON/OFF
/* [CCM] 클럭 제어 모듈 추가 */
#define CCM_CCGR3                               (*(volatile uint32_t *)0x400FC074) // SEMC 모듈 전원/클럭 활성화
#define CCM_CCGR5                               (*(volatile uint32_t *)0x400FC07C) // Clock Gating Reg 5 — LPUART1 등 모듈 클럭 ON/OFF

/* [IOMUXC] 핀 다중화 모듈 — 물리 핀을 어떤 기능(ALT)으로 쓸지 선택 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0) // LED
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06     (*(volatile uint32_t *)0x401F80D4) // UART1 TX
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07     (*(volatile uint32_t *)0x401F80D8) // UART1 RX

/* [GPIO1] 범용 입출력 모듈 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000) // Data Register — 핀 출력 HIGH/LOW 값
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004) // Direction Register — 핀 입력(0)/출력(1) 방향

/* [LPUART1] 시리얼 통신 모듈 */
#define LPUART1_BAUD                            (*(volatile uint32_t *)0x40184010) // Baud Rate Register — OSR/SBR 통신 속도 설정
#define LPUART1_STAT                            (*(volatile uint32_t *)0x40184014) // Status Register — TDRE/RDRF 등 송수신 상태 플래그
#define LPUART1_CTRL                            (*(volatile uint32_t *)0x40184018) // Control Register — TE/RE/RIE 송수신·인터럽트 활성화
#define LPUART1_DATA                            (*(volatile uint32_t *)0x4018401C) // Data Register — 송신/수신 데이터 출입구

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

/* ⚠️ 주의: 위의 다른 매크로들과 달리 이것은 일부러 역참조(*(volatile uint32_t *))를
 *          하지 않은 "순수 주소값"이다.
 *  - 이유: EMC_00 의 MUX 레지스터를 베이스로 삼아 EMC_00~EMC_41 을 배열처럼 접근하기 위함.
 *          semc.c 에서 (volatile uint32_t *)IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00 로
 *          캐스팅한 뒤 emc_mux_base[i] 형태로 사용한다.
 *  - 따라서 다른 매크로처럼 'IOMUXC_..._EMC_00 = 0;' 으로 쓰면 안 된다 (오동작). */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00       (0x401F8014)

#endif // RT1020_REGS_H
