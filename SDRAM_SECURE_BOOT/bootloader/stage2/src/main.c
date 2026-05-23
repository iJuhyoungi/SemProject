#include <stdint.h>
#include "uart.h"
#include "led.h"

/**
 * Stage 2 — Secure Boot 의 검증 대상 image (demo payload).
 *
 * 이 binary 는 "Stage 1 이 무엇을 검증하는가" 의 대상일 뿐, 하는 일은
 * 최소화한다. 호스트 서명 도구(Step 3)가 이 .bin 을 서명해 첨부하고,
 * Stage 1 이 검증 통과 시 여기로 점프한다.
 *
 * LED 깜빡임 = 정상 실행 신호 (Stage 1 halt 시의 상시 점등과 구분).
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
    UART1_SendString("\r\n-----------------------------\r\n");
    UART1_SendString("[BL2] Stage 2 verified & running\r\n");
    UART1_SendString("-----------------------------\r\n");

    LED_Init();
    while(1) {
        LED_On();
        delay_loop(2000000);
        LED_Off();
        delay_loop(2000000);
    }

}
