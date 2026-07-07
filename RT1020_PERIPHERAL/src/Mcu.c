#include "Mcu.h"
#include "lpspi.h"
#include "rt1020_regs.h"

#define SRSR_POR  (1u << 0)     /* ipp_reset_b  */
#define SRSR_WDG3 (1u << 7)     /* wdog3_rst_b  */

/* 레벨1: 이미지가 ROM 기본 클럭(IS_BOOTLOADER) 사용 → 시스템 클럭 별도 설정 없음 */
void Mcu_Init(void)
{
}

/* 주변장치 클럭 게이트. 실무 Mcu 는 PLL·클럭트리 전체 관리 */
void Mcu_InitClock(void)
{
    LPSPI1_Clock_Enable();
}

/* 리셋 원인 판별 — Wdg 에서 이사 옴 (스펙상 리셋 원인은 Mcu 소관) */
Mcu_ResetType Mcu_GetResetReason(void)
{
    uint32_t srsr = SRC_SRSR;

    if (srsr & SRSR_WDG3) {
        return MCU_WATCHDOG_RESET;      /* POR 비트와 같이 떠도 워치독 우선 판별 */
    }
    if (srsr & SRSR_POR) {
        return MCU_POWER_ON_RESET;
    }
    return MCU_RESET_UNDEFINED;
}

uint32_t Mcu_GetResetRawValue(void)
{
    return SRC_SRSR;
}

void Mcu_ClearResetReason(void)
{
    SRC_SRSR = SRC_SRSR;                /* W1C: 떠 있는 비트 전부 클리어 */
}
