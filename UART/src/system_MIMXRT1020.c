#include <stdint.h>

#include "mimxrt1020.h"
#include "system_MIMXRT1020.h"

extern uint32_t __VECTOR_TABLE[];

uint32_t SystemCoreClock = 500000000U;

void SystemInit(void)
{
    /* 1. NXP 하드웨어 워치독 강제 종료 (가장 중요!!!) */
    
    // WDOG1, WDOG2 비활성화 (WDE 비트 클리어)
    *((volatile uint16_t *)0x400B8000) &= ~0x04; 
    *((volatile uint16_t *)0x400D0000) &= ~0x04; 

    // RTWDOG 비활성화 (특수 암호 입력 필요)
    *((volatile uint32_t *)0x400BC004) = 0xD928C520; // Update Key 입력
    *((volatile uint32_t *)0x400BC008) = 0xFFFF;     // Timeout 값 최대화
    // CS 레지스터 수정하여 Watchdog 비활성화
    *((volatile uint32_t *)0x400BC000) = (*((volatile uint32_t *)0x400BC000) & ~0x80) | 0x20; 

    /* 2. FPU 및 벡터 테이블 설정 (기존 코드) */
    SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));
    SCB->VTOR = (uint32_t)(uintptr_t)__VECTOR_TABLE;
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
