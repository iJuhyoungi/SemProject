#include <stdint.h>
#include "uart.h"
#include "led.h"

/* busy-wait */
static void delay_busy(volatile uint32_t n){
    while(n--){
        __asm volatile("nop");
    }
}

int main(void) {
    /* UART1은 startup의 IS_BOOTLOADER 분기가 이미 init 함 — 바로 출력 가능 */
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[PERI] RT1020 peripheral bring-up\r\n");
    UART1_SendString("[PERI] hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    uint32_t beat=0;
    while(1){
        LED_On();
        delay_busy(50000000);
        LED_Off();
        delay_busy(50000000);

        UART1_SendString("[PERI] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}