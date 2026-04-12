#include <stdint.h>
#include "uart.h"
#include "led.h"

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

int main(void)
{
    UART1_Init();
    LED_Init();

    UART1_SendString("\r\n[APP-B] Dummy app started\r\n");
    UART1_SendString("[APP-B] boot confirmed hook point\r\n");

    while (1) {
        LED_On();
        UART1_SendString("[APP-B] heartbeat\r\n");
        delay_loop(1500000);

        LED_Off();
        delay_loop(1500000);
    }
}