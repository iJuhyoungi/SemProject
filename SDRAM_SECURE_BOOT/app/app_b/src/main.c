#include <stdint.h>
#include "uart.h"
#include "led.h"

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; ++i)
        __asm("nop");
}

int main(void)
{
    UART1_SendString("\r\n========================\r\n");
    UART1_SendString("[App B] running (slot B)\r\n");
    UART1_SendString("========================\r\n");

    LED_Init();
    while (1)
    {
        /* App B 시그니처: 3회 빠른 깜빡 후 긴 쉼 */
        LED_On();
        delay_loop(400000);
        LED_Off();
        delay_loop(400000);
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
