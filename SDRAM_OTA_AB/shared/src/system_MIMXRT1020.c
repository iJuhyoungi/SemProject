// #include <stdint.h>

// #include "mimxrt1020.h"
// #include "system_MIMXRT1020.h"
// #include "mpu.h"

// extern uint32_t __VECTOR_TABLE[];

// uint32_t SystemCoreClock = 500000000U;

// __attribute__((section(".boot_text"))) void SystemInit(void)
// {
//     /* 1. NXP 하드웨어 워치독 강제 종료 (가장 중요!!!) */
    
//     // WDOG1, WDOG2 비활성화 (WDE 비트 클리어)
//     *((volatile uint16_t *)0x400B8000) &= ~0x04; 
//     *((volatile uint16_t *)0x400D0000) &= ~0x04; 

//     // RTWDOG 비활성화 (특수 암호 입력 필요)
//     *((volatile uint32_t *)0x400BC004) = 0xD928C520; // Update Key 입력
//     *((volatile uint32_t *)0x400BC008) = 0xFFFF;     // Timeout 값 최대화
//     // CS 레지스터 수정하여 Watchdog 비활성화
//     *((volatile uint32_t *)0x400BC000) = (*((volatile uint32_t *)0x400BC000) & ~0x80) | 0x20; 

//     /* 🚀 2. MPU 초기화 (캐시를 켜기 위한 메모리 토대 마련) */
//     MPU_Init();

//     /* 3. FPU 및 벡터 테이블 설정 (기존 코드) */
//     SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));
//     SCB->VTOR = (uint32_t)(uintptr_t)__VECTOR_TABLE;
// }

// void SystemCoreClockUpdate(void)
// {
//     /*
//      * This template assumes the default RT1020 boot clocking path with the
//      * core running at 500 MHz. Update this function after adding board-level
//      * clock initialization.
//      */
//     SystemCoreClock = 500000000U;
// }

#include <stdint.h>
#include "mimxrt1020.h"
#include "system_MIMXRT1020.h"
#include "mpu.h"

extern uint32_t __VECTOR_TABLE[];

uint32_t SystemCoreClock = 500000000U;

/* WDOG1 / WDOG2 base */
#define WDOG1_WCR   (*(volatile uint16_t *)0x400B8000)
#define WDOG1_WMCR  (*(volatile uint16_t *)0x400B8008)

#define WDOG2_WCR   (*(volatile uint16_t *)0x400D0000)
#define WDOG2_WMCR  (*(volatile uint16_t *)0x400D0008)

/* RTWDOG (WDOG3) base */
#define RTWDOG_CS    (*(volatile uint32_t *)0x400BC000)
#define RTWDOG_CNT   (*(volatile uint32_t *)0x400BC004)
#define RTWDOG_TOVAL (*(volatile uint32_t *)0x400BC008)

/* RTWDOG CS bits */
#define RTWDOG_CS_EN      (1u << 7)
#define RTWDOG_CS_UPDATE  (1u << 5)
#define RTWDOG_CS_RCS     (1u << 10)
#define RTWDOG_CS_ULK     (1u << 11)

__attribute__((section(".boot_text"))) void SystemInit(void)
{
    __asm volatile ("cpsid i");

    /* -------------------------------------------------
     * WDOG1 / WDOG2
     * ------------------------------------------------- */
    /* Power-down counter disable */
    WDOG1_WMCR = 0x0000;
    WDOG2_WMCR = 0x0000;

    /* Disable WDE, set debug/wait/stop suspend bits early */
    WDOG1_WCR = 0x0030;   /* WDE=0, default-safe disable state */
    WDOG2_WCR = 0x0030;

    /* -------------------------------------------------
     * RTWDOG (WDOG3)
     * RM 53.4.2 style disable-after-reset
     * ------------------------------------------------- */
    RTWDOG_CNT = 0xD928C520;      /* unlock */
    RTWDOG_TOVAL = 0xFFFF;
    RTWDOG_CS &= ~RTWDOG_CS_EN;   /* disable */

    while (RTWDOG_CS & RTWDOG_CS_ULK) {
        /* wait until lock sequence finishes */
    }
    while ((RTWDOG_CS & RTWDOG_CS_RCS) == 0u) {
        /* wait until new configuration takes effect */
    }

    __asm volatile ("dsb");
    __asm volatile ("isb");

    MPU_Init();

    SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));
    SCB->VTOR = (uint32_t)(uintptr_t)__VECTOR_TABLE;

    __asm volatile ("cpsie i");
}

void SystemCoreClockUpdate(void)
{
    /*
     * This template assumes the default RT1020 boot clocking path with the
     * core running at 500 MHz. Update this function after adding board-level
     * clock initialization.
     */
    SystemCoreClock = 500000000U;
}

// __attribute__((section(".boot_text")))
// void HardFault_Handler(void)
// {
//     UART1_SendString("[FAULT] HardFault\r\n");
//     UART1_SendString("CFSR=0x");
//     UART1_SendHex32(SCB->CFSR);
//     UART1_SendString("\r\n");

//     UART1_SendString("HFSR=0x");
//     UART1_SendHex32(SCB->HFSR);
//     UART1_SendString("\r\n");

//     UART1_SendString("BFAR=0x");
//     UART1_SendHex32(SCB->BFAR);
//     UART1_SendString("\r\n");

//     UART1_SendString("MMFAR=0x");
//     UART1_SendHex32(SCB->MMFAR);
//     UART1_SendString("\r\n");

//     while (1) {}
// }

// __attribute__((section(".boot_text")))
// void BusFault_Handler(void)
// {
//     UART1_SendString("[FAULT] BusFault\r\n");
//     UART1_SendString("CFSR=0x");
//     UART1_SendHex32(SCB->CFSR);
//     UART1_SendString("\r\n");
//     UART1_SendString("BFAR=0x");
//     UART1_SendHex32(SCB->BFAR);
//     UART1_SendString("\r\n");
//     while (1) {}
// }

// __attribute__((section(".boot_text")))
// void MemManage_Handler(void)
// {
//     UART1_SendString("[FAULT] MemManage\r\n");
//     UART1_SendString("CFSR=0x");
//     UART1_SendHex32(SCB->CFSR);
//     UART1_SendString("\r\n");
//     UART1_SendString("MMFAR=0x");
//     UART1_SendHex32(SCB->MMFAR);
//     UART1_SendString("\r\n");
//     while (1) {}
// }
