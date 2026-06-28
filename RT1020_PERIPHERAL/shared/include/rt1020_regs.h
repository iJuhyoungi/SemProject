#ifndef RT1020_REGS_H
#define RT1020_REGS_H

#include <stdint.h>

/* [CCM] 클럭 제어 모듈 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
#define CCM_CSCDR1                              (*(volatile uint32_t *)0x400FC024)
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)  // LPSPI1~4, SEMC_EXSC, GPT1, LPUART4, GPIO1, CSU, GPIO5
/* [CCM] 클럭 제어 모듈 추가 */
#define CCM_CCGR0                               (*(volatile uint32_t *)0x400FC068)  // CCGR0: CG7 ipg=bits15:14, CG8 PE=bits17:16 / CSCMR2 CAN_CLK_SEL=bits9:8
#define CCM_CCGR3                               (*(volatile uint32_t *)0x400FC074)  // SEMC 모듈 전원/클럭 활성화, LPUART5~6, AOI1, ENC1~2, PWM1~2 
#define CCM_CCGR4                               (*(volatile uint32_t *)0x400FC078)
#define CCM_CCGR5                               (*(volatile uint32_t *)0x400FC07C)  // DMA + DMAMUX(CG3), FLEXSPI, TRNG, USDHC2, LPUART8, DCDC

#define CCM_CSCMR2                              (*(volatile uint32_t *)0x400FC020)

/* [IOMUXC] 핀 다중화 모듈 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0) // LED
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06     (*(volatile uint32_t *)0x401F80D4) // UART1 TX
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07     (*(volatile uint32_t *)0x401F80D8) // UART1 RX
/* IOMUXC (RM Ch.11), base 0x401F_8000 — LPSPI1 핀 (전부 ALT1) */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_10     (*(volatile uint32_t *)0x401F80E4) // LPSPI1 SCK
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_11     (*(volatile uint32_t *)0x401F80E8) // LPSPI1 PCS0
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_12     (*(volatile uint32_t *)0x401F80EC) // LPSPI1 SDO
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_13     (*(volatile uint32_t *)0x401F80F0) // LPSPI1 SDI
#define IOMUXC_SW_PAD_CTL_PAD_GPIO_SD_B0_02     (*(volatile uint32_t *)0x401F82B8) // LPSPI1 SDI 선택 입력 레지스터

/* [GPIO1] 범용 입출력 모듈 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)

/* [LPUART1] 시리얼 통신 모듈 */
#define LPUART_STAT_RDRF_MASK   (1u << 21)      /* Receive Data Register Full */
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


#define SDRAM_BASE_ADDR              (0x80000000u)

/* [NVIC] Nested Vectored Interrupt Controller */
#define NVIC_ISER0                              (*(volatile uint32_t *)0xE000E100) // 인터럽트 허용 레지스터 0
#define NVIC_ISER1                              (*(volatile uint32_t *)0xE000E104) // NVIC (ARM Cortex-M7) — IRQ 32~63 set-enable
#define NVIC_ISER3                              (*(volatile uint32_t *)0xE000E10C) // IRQ 96~127 set enable
#define NVIC_ICER0                              (*(volatile uint32_t *)0xE000E180) // 인터럽트 금지 레지스터 0
#define NVIC_ICPR0                              (*(volatile uint32_t *)0xE000E280) // 인터럽트 클리어 레지스터 0
#define SYST_CSR                                (*(volatile uint32_t *)0xE000E010) // SysTick 제어 및 상태 레지스터

/* ARM Cortex-M7 MPU 레지스터 구조체 (CMSIS 호환) */
#define MPU_TYPE    (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL    (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR     (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR    (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR    (*(volatile uint32_t *)0xE000EDA0)


/* ARM Cortex-M7 SCB (System Control Block) 레지스터 주소 직접 매핑 */
#define SCB_AIRCR       (*(volatile uint32_t *)0xE000ED0C)
#define SCB_CCR         (*(volatile uint32_t *)0xE000ED14)
#define SCB_ICIALLU     (*(volatile uint32_t *)0xE000ED50)
#define SCB_VTOR        (*(volatile uint32_t *)0xE000ED08)

/* AIRCR Helper */
#define AIRCR_VECTKEY  (0x05FAu << 16)                                                                                                                                                   
#define AIRCR_SYSRESETREQ  (1u << 2)

/* D-Cache 제어용 특수 레지스터 */
#define SCB_CCSIDR      (*(volatile uint32_t *)0xE000ED80) /* Cache Size ID */
#define SCB_CSSELR      (*(volatile uint32_t *)0xE000ED84) /* Cache Size Select */
#define SCB_DCISW       (*(volatile uint32_t *)0xE000ED60) /* D-Cache Invalidate by Set-Way */
#define SCB_DCCISW      (*(volatile uint32_t *)0xE000ED74) /* D-Cache Clean & Invalidate */
#define SCB_DCCMVAC     (*(volatile uint32_t *)0xE000EF68) /* D-Cache Clean by Address */
#define SCB_DCIMVAC     (*(volatile uint32_t *)0xE000EF5C) /* D-Cache Invalidate by Address */


/* 🚀 ARM Cortex-M7 데이터 워치포인트 및 추적(DWT) 레지스터 
 * (CPU 클럭 사이클을 1클럭 단위로 정확하게 셀 수 있는 하드웨어 타이머) */
#define CM_DEMCR (*(volatile uint32_t *)0xE000EDFC)                 // bit24 TRCENA
#define DWT_CTRL        (*(volatile uint32_t *)0xE0001000)          // bit0 CYCCNTENA
#define DWT_CYCCNT      (*(volatile uint32_t *)0xE0001004)

#define EDMA_BASE 0x400E8000
#define EDMA_CR   (*(volatile uint32_t *)(EDMA_BASE + 0x000))
#define EDMA_ES   (*(volatile uint32_t *)(EDMA_BASE + 0x004)) /* Error Status */
#define EDMA_ERQ  (*(volatile uint32_t *)(EDMA_BASE + 0x00C)) /* Enable Request */

/* eDMA 채널0 TCD (= EDMA_BASE + 0x1000 = 0x400E9000) */
#define EDMA_TCD0_BASE       (EDMA_BASE+0x1000)

#define EDMA_TCD0_SADDR      (*(volatile uint32_t *)(EDMA_TCD0_BASE + 0x00))
#define EDMA_TCD0_SOFF       (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x04))
#define EDMA_TCD0_ATTR       (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x06))
#define EDMA_TCD0_NBYTES     (*(volatile uint32_t *)(EDMA_TCD0_BASE + 0x08))
#define EDMA_TCD0_SLAST      (*(volatile uint32_t *)(EDMA_TCD0_BASE + 0x0C))
#define EDMA_TCD0_DADDR      (*(volatile uint32_t *)(EDMA_TCD0_BASE + 0x10))
#define EDMA_TCD0_DOFF       (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x14))
#define EDMA_TCD0_CITER      (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x16))
#define EDMA_TCD0_DLASTSGA   (*(volatile uint32_t *)(EDMA_TCD0_BASE + 0x18))
#define EDMA_TCD0_CSR        (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x1C))
#define EDMA_TCD0_BITER      (*(volatile uint16_t *)(EDMA_TCD0_BASE + 0x1E))

/* 🚀 진짜 NXP i.MX RT1020 eDMA 8-bit 제어 레지스터 주소 (TRM 검증 완료) */
#define EDMA_CERQ           (*(volatile uint8_t *)(EDMA_BASE + 0x01A))
#define EDMA_SERQ           (*(volatile uint8_t *)(EDMA_BASE + 0x01B))
#define EDMA_CDNE           (*(volatile uint8_t *)(EDMA_BASE + 0x01C))      // Clear DONE
#define EDMA_SSRT           (*(volatile uint8_t *)(EDMA_BASE + 0x01D))      // Set START
#define EDMA_CERR           (*(volatile uint8_t *)(EDMA_BASE + 0x01E))      // Clear ERROR
#define EDMA_INT            (*(volatile uint32_t *)(EDMA_BASE + 0x024))     // 채널별 INT 플래그 (W1C)
#define EDMA_CINT           (*(volatile uint8_t *)(EDMA_BASE + 0x01F))     // 채널번호 write = INT clear


/* DMAMUX (eDMA의 문지기) 베이스 주소 */
#define DMAMUX_BASE 0x400EC000
#define DMAMUX_CHCFG0 (*(volatile uint32_t *)(DMAMUX_BASE + 0x000))


/* [IOMUXC] SEMC 핀 MUX 시작 주소 */

/*
 **Chapter 27 (SEMC)**를 보고: "아, 이 칩에서 SDRAM을 돌리려면 SEMC라는 엔진을 써야 하는구나."
 **Chapter 10 (Pin Muxing)**을 보고: "SEMC 엔진의 신호를 밖으로 내보내려면 GPIO_EMC_xx 핀들을 써야 하고, 이때 ALT0 모드를 골라야 하는구나."
 **Chapter 11 (IOMUXC)**를 보고: "그러므로 IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_xx 레지스터 값을 0으로 세팅하면 물리적으로 SDRAM 컨트롤러와 핀이 연결되겠구나!"
 */

#define IOMUXC_SW_MUX_CTL_PAD_GPIO_EMC_00       (0x401F8014)


#define LPSPI1_BASE         0x40394000u
#define LPSPI1_VERID        (*(volatile uint32_t *)(LPSPI1_BASE + 0x000))
#define LPSPI1_CR           (*(volatile uint32_t *)(LPSPI1_BASE+0x10))
#define LPSPI1_SR           (*(volatile uint32_t *)(LPSPI1_BASE+0x14))
#define LPSPI1_IER          (*(volatile uint32_t *)(LPSPI1_BASE+0x18))
#define LPSPI1_DER          (*(volatile uint32_t *)(LPSPI1_BASE+0x1C))
#define LPSPI1_CFGR1        (*(volatile uint32_t *)(LPSPI1_BASE+0x24))
#define LPSPI1_CCR          (*(volatile uint32_t *)(LPSPI1_BASE+0x40))
#define LPSPI1_FCR          (*(volatile uint32_t *)(LPSPI1_BASE+0x58))
#define LPSPI1_FSR          (*(volatile uint32_t *)(LPSPI1_BASE+0x5C))
#define LPSPI1_TCR          (*(volatile uint32_t *)(LPSPI1_BASE+0x60))
#define LPSPI1_TDR          (*(volatile uint32_t *)(LPSPI1_BASE+0x64))
#define LPSPI1_RDR          (*(volatile uint32_t *)(LPSPI1_BASE+0x74))
#define LPSPI1_PARAM        (*(volatile uint32_t *)(LPSPI1_BASE+0x04))

/* ADC1 register(base : 0x400C4000) */
#define ADC1_BASE           0x400C4000u
#define ADC1_HC0            (*(volatile uint32_t *)(ADC1_BASE+0x00))
#define ADC1_HS             (*(volatile uint32_t *)(ADC1_BASE+0x20))
#define ADC1_R0             (*(volatile uint32_t *)(ADC1_BASE+0x24))
#define ADC1_CFG            (*(volatile uint32_t *)(ADC1_BASE+0x44))
#define ADC1_GC             (*(volatile uint32_t *)(ADC1_BASE+0x48))
#define ADC1_GS             (*(volatile uint32_t *)(ADC1_BASE+0x4C))

/* FLEXPWM1 (RM ch.50), base 0x403DC000 */
#define FLEXPWM1_BASE       0x403DC000u
#define FLEXPWM1_SM0CNT     (*(volatile uint16_t *)(FLEXPWM1_BASE+0x00))
#define FLEXPWM1_SM0INIT    (*(volatile uint16_t *)(FLEXPWM1_BASE+0x02))
#define FLEXPWM1_SM0CTRL    (*(volatile uint16_t *)(FLEXPWM1_BASE+0x06))
#define FLEXPWM1_SM0VAL1    (*(volatile uint16_t *)(FLEXPWM1_BASE+0x0E))
#define FLEXPWM1_SM0VAL2    (*(volatile uint16_t *)(FLEXPWM1_BASE+0x12))
#define FLEXPWM1_SM0VAL3    (*(volatile uint16_t *)(FLEXPWM1_BASE+0x16))
#define FLEXPWM1_OUTEN      (*(volatile uint16_t *)(FLEXPWM1_BASE+0x180))
#define FLEXPWM1_MCTRL      (*(volatile uint16_t *)(FLEXPWM1_BASE+0x188))

/* FLEXCAN1 (RM Ch.40), base 0x401D0000 */
#define FLEXCAN1_BASE       0x401D0000u
#define FLEXCAN1_MCR        (*(volatile uint32_t *)(FLEXCAN1_BASE+0x00))
#define FLEXCAN1_CTRL1      (*(volatile uint32_t *)(FLEXCAN1_BASE+0x04))

/* 메시지 버퍼 영역 : base + 0x80, 각각은 16바이트 */
#define FLEXCAN1_MB(n)      ((volatile uint32_t *)(FLEXCAN1_BASE+0x80+(n)*16u))
#define FLEXCAN1_TIMER      (*(volatile uint32_t *)(FLEXCAN1_BASE+0x08))        // MB unlock
#define FLEXCAN1_IFLAG1     (*(volatile uint32_t *)(FLEXCAN1_BASE+0x30))        // MB 완료 flag

// GPT1 : Base 0x401EC000
#define GPT1_BASE           0x401EC000u
#define GPT1_CR             (*(volatile uint32_t *)(GPT1_BASE+0x00))            // EN[0]/ENMOD[1]/CLKSRC[8:6]/FRR[9]/SWR[15]
#define GPT1_PR             (*(volatile uint32_t *)(GPT1_BASE+0x04))            // PRESCALER[11:0]
#define GPT1_SR             (*(volatile uint32_t *)(GPT1_BASE+0x08))            // OF1[0]..ROV[5], W1C
#define GPT1_IR             (*(volatile uint32_t *)(GPT1_BASE+0x0C))            // OF1IE[0]..ROVIE[5]
#define GPT1_OCR1           (*(volatile uint32_t *)(GPT1_BASE+0x10))            // 비교값 = 주기
#define GPT1_CNT            (*(volatile uint32_t *)(GPT1_BASE+0x24))            // 현재 카운터 (RO)

// RTWDOG(WDOG3, RM Ch.53) Base : 0x400BC000
#define RTWDG_BASE          0x400BC000u
#define RTWDG_CS            (*(volatile uint32_t *)(RTWDG_BASE+0x00))           // 제어/상태 (대부분은 write-once)
#define RTWDG_CNT           (*(volatile uint32_t *)(RTWDG_BASE+0x04))           // refresh/unlock 명령 레지스터
#define RTWDG_TOVAL         (*(volatile uint32_t *)(RTWDG_BASE+0x08))           // timeout value
#define RTWDG_WIN           (*(volatile uint32_t *)(RTWDG_BASE+0x0C))           // window 하한

// SRC (System Reset Controller) : reset cause
#define SRC_SRSR            (*(volatile uint32_t *)0x400F8008u)                 // wdg3_rst_b=bit7, W1C
//Wdg3 게이트는 CCM_CCGR5의 CG2(bits[5:4])

#endif // RT1020_REGS_H
