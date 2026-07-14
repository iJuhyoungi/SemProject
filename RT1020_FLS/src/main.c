#include <stdint.h>
#include "uart.h"
#include "led.h"

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[FLS] RT1020 Flash Driver — hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    /* F-0: 부팅 체인(ROM->FCB->IVT->Reset_Handler->main->UART) 확인용 심장박동 */
    uint32_t beat = 0;
    while (1)
    {
        LED_On();
        delay_busy(20000000);
        LED_Off();
        delay_busy(20000000);

        UART1_SendString("[FLS] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}
