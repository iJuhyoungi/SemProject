#include <stdint.h>
#include "uart.h"
#include "led.h"

/*
* Recovery application.
*
* 책임 (Phase 4 Step 1 — passive recovery):
*  1. UART 메시지 출력 — Stage 2 invalid 로 recovery 진입했음을 알림
*  2. LED 깜빡 — 시각적 신호
*  3. halt — 사용자가 reflash 또는 SW7 reset 까지 대기
*
* 향후 (Phase 4 Step 4 — active recovery):
*  - UART download protocol 로 새 Stage 2 binary 받기
*  - Stage 2 영역 erase + program
*  - reset → 정상 부팅 path 재개
*
* 메모리 위치: 0x603E0000 (64 KB 영역)
*/

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; ++i)
    {
        __asm("nop");
    }
}

int main(void)
{
    UART1_SendString("\r\n========================================\r\n");
    UART1_SendString(" [RECOVERY] Stage 2 invalid — recovery active\r\n");
    UART1_SendString("========================================\r\n");
    UART1_SendString("[RECOVERY] Please reflash via flash_mcu.sh\r\n");

    LED_Init();

    while (1)
    {
        LED_On();
        delay_loop(5000000);
        LED_Off();
        delay_loop(5000000);
    }
}