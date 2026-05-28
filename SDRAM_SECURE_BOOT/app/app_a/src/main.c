#include <stdint.h>
#include "uart.h"
#include "led.h"

/*
 * App A — Stage 2 가 검증하고 점프하는 최종 application (demo).
 * LED 2회 깜빡 패턴 + UART 로 "어느 partition 이 부팅됐는지" 표시.
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
    UART1_SendString("\r\n========================\r\n");
    UART1_SendString("[App A] running (slot A)\r\n");
    UART1_SendString("========================\r\n");

    LED_Init();
    while (1)
    {
        LED_On();
        delay_loop(400000);
        LED_Off();
        delay_loop(400000);

        LED_On();
        delay_loop(400000);
        LED_Off();
        delay_loop(2400000);
    }
}
