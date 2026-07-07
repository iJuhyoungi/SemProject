#ifndef MCU_H
#define MCU_H

#include <stdint.h>

void Mcu_Init(void);
void Mcu_InitClock(void);

/* --- 리셋 원인 (SWS_Mcu 8장: GetResetReason SID 0x05 / GetResetRawValue SID 0x06) --- */
typedef enum
{
    MCU_POWER_ON_RESET = 0,     /* SRSR bit0 (ipp_reset_b) */
    MCU_WATCHDOG_RESET,         /* SRSR bit7 (wdog3_rst_b) */
    MCU_SW_RESET,               /* 레벨1 미사용 */
    MCU_RESET_UNDEFINED
} Mcu_ResetType;

Mcu_ResetType Mcu_GetResetReason(void);
uint32_t      Mcu_GetResetRawValue(void);   /* SRSR 원본 (진단 출력용) */
void          Mcu_ClearResetReason(void);   /* SRSR W1C — 자체 helper (스펙에선 Mcu_Init 이 수행) */

#endif
