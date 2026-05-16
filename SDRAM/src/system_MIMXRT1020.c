/*
 * system_MIMXRT1020.c
 * -----------------------------------------------------------------------------
 * SystemInit() — 스타트업 코드가 main() 진입 직전에 호출하는 칩 최소 초기화.
 *
 * 베어메탈에서 가장 먼저 해야 할 일 중 하나는 워치독을 끄는 것이다.
 * 워치독을 주기적으로 리셋(kick)하는 코드가 아직 없으므로, 그대로 두면
 * 몇십~몇백 ms 후 칩이 스스로 리셋되어 무한 재부팅에 빠진다.
 * -----------------------------------------------------------------------------
 */
#include <stdint.h>

#include "mimxrt1020.h"
#include "system_MIMXRT1020.h"

/* 스타트업 파일이 정의하는 벡터 테이블의 시작 주소 (VTOR 에 넣기 위함). */
extern uint32_t __VECTOR_TABLE[];

/* CMSIS 표준 전역 변수 — 현재 코어 클럭(Hz)을 보관 (정보용). */
uint32_t SystemCoreClock = 500000000U;

void SystemInit(void)
{
    /* 1. NXP 하드웨어 워치독 강제 종료 (가장 중요!!!) */

    /* WDOG1, WDOG2 비활성화 — WCR(Watchdog Control Register)의 WDE 비트(비트2, 0x04) 클리어.
     *   WDOG1 WCR = 0x400B8000, WDOG2 WCR = 0x400D0000 (16비트 레지스터). */
    *((volatile uint16_t *)0x400B8000) &= ~0x04;
    *((volatile uint16_t *)0x400D0000) &= ~0x04;

    /* RTWDOG 비활성화 — 이 워치독은 잠금이 걸려 있어 특수 시퀀스가 필요하다.
     *   0x400BC004 (CNT): 언락 키 0xD928C520 입력 -> 일시적으로 레지스터 쓰기 허용
     *   0x400BC008 (TOVAL): 타임아웃 값을 최대(0xFFFF)로
     *   0x400BC000 (CS):   EN 비트(0x80) 클리어로 워치독 끄고, UPDATE 비트(0x20) 셋 */
    *((volatile uint32_t *)0x400BC004) = 0xD928C520; // Update Key 입력
    *((volatile uint32_t *)0x400BC008) = 0xFFFF;     // Timeout 값 최대화
    // CS 레지스터 수정하여 Watchdog 비활성화
    *((volatile uint32_t *)0x400BC000) = (*((volatile uint32_t *)0x400BC000) & ~0x80) | 0x20;

    /* 2. FPU 및 벡터 테이블 설정 (기존 코드) */

    /* FPU 활성화 — CPACR 의 CP10/CP11 필드(비트 23:20)를 11,11 로 설정해 풀 액세스 허용.
     * Cortex-M7 FPU 는 리셋 직후 꺼져 있어, 켜기 전에 float 명령을 쓰면 HardFault. */
    SCB->CPACR |= ((3UL << 20U) | (3UL << 22U));

    /* 인터럽트 벡터 테이블 위치를 VTOR 에 등록.
     * 코드가 외부 Flash 대역에 있으므로 기본값(0x0)이 아닌 실제 주소를 알려줘야 한다. */
    SCB->VTOR = (uint32_t)(uintptr_t)__VECTOR_TABLE;
}

/* CMSIS 표준 함수 — SystemCoreClock 값을 현재 클럭 설정에 맞춰 갱신.
 * 현재는 RT1020 기본 부팅 클럭(코어 500MHz)을 가정한 템플릿 상태. */
void SystemCoreClockUpdate(void)
{
    /*
     * This template assumes the default RT1020 boot clocking path with the
     * core running at 500 MHz. Update this function after adding board-level
     * clock initialization.
     */
    SystemCoreClock = 500000000U;
}
