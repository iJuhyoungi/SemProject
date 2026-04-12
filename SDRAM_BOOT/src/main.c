#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "led.h"
#include "semc.h"

/* 공용 딜레이 함수 */
void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

int main(void)
{
    /* 초기 지연 (디버거 연결 및 안정화) */
    delay_loop(2000000);

    /* 기본 I/O 초기화 (클럭은 이미 Reset_Handler에서 132MHz로 세팅됨) */
    LED_Init();
    UART1_Init();

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" Phase 1.2: XIP to SDRAM Boot Success!\r\n");
    UART1_SendString("=================================\r\n");

    /* 💡 핵심 증명: 현재 실행 중인 main 함수의 메모리 주소를 출력합니다. 
     * 이 주소가 0x8000XXXX 대역으로 찍힌다면, 컴파일러를 속이는 링커 수술과 
     * 부트로더의 메모리 점프가 완벽하게 성공한 것입니다! */
    UART1_SendString(" -> Current 'main' function address : ");
    UART1_SendHex32((uint32_t)&main);
    UART1_SendString("\r\n");

    /* 🚨 주의: Reset_Handler의 부트로더 과정에서 
     * 이미 SDRAM_Clock_Init, SDRAM_Pin_Init, SDRAM_Init_Sequence를 모두 끝냈으므로 
     * 여기서 다시 호출하면 안 됩니다! 바로 메모리 테스트로 진입합니다. */

    UART1_SendString("\r\n[1] Starting SDRAM Integrity Tests from SDRAM...\r\n");

    if (!SDRAM_Test16()) {
        UART1_SendString(" -> Stop after 16-bit failure.\r\n");
        while (1) {
            __asm("wfi");
        }
    }

    if (!SDRAM_Test32()) {
        UART1_SendString(" -> Stop after 32-bit failure.\r\n");
        while (1) {
            __asm("wfi");
        }
    }

    UART1_SendString("\r\n[Done] All SDRAM test routines finished flawlessly.\r\n");
    UART1_SendString(" -> The core is running 100% on SDRAM now!\r\n");

    while (1) {
        /* LED Toggle Test (옵션) */
        // GPIO1_DR ^= (1 << 5);
        // delay_loop(10000000);
        __asm("wfi");
    }
    
    return 0;
}