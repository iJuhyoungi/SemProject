#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "led.h"
#include "semc.h"
#include "cache.h"
#include "rt1020_regs.h"

/* 공용 딜레이 함수 */
void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

/* 🚀 256KB(65536 words) SDRAM 메모리 복사 벤치마크 함수 */
void Run_Memcopy_Benchmark(const char* test_name)
{
    volatile uint32_t *src = (volatile uint32_t *)(SDRAM_BASE_ADDR + 0x01000000); // 16MB 지점
    volatile uint32_t *dst = (volatile uint32_t *)(SDRAM_BASE_ADDR + 0x01100000); // 17MB 지점
    const uint32_t word_count = 65536; // 256KB 

    /* 테스트 데이터 초기화 */
    for(uint32_t i = 0; i < word_count; i++) {
        src[i] = i;
    }

    /* 사이클 카운터 초기화 및 시작 */
    CoreDebug_DEMCR |= (1 << 24); // TRCENA 활성화
    DWT_CYCCNT = 0;               // 카운터 0으로 리셋
    DWT_CTRL |= 1;                // 카운터 작동 시작

    /* ⏱️ 시간 측정 시작: 복사! */
    uint32_t start_cycle = DWT_CYCCNT;
    
    for(uint32_t i = 0; i < word_count; i++) {
        dst[i] = src[i];
    }
    
    uint32_t end_cycle = DWT_CYCCNT;
    /* ⏱️ 시간 측정 종료 */

    uint32_t elapsed_cycles = end_cycle - start_cycle;

    /* 결과 출력 */
    UART1_SendString(test_name);
    UART1_SendString(" - Cycles: 0x");
    UART1_SendHex32(elapsed_cycles);
    UART1_SendString("\r\n");
}

int main(void)
{
    delay_loop(2000000);

    LED_Init();
    UART1_Init();

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" Phase 1.4: D-Cache Benchmark Test\r\n");
    UART1_SendString("=================================\r\n");

    /* I-Cache는 기본적으로 켜고 시작 (명령어 속도는 보장) */
    ICache_Enable();
    UART1_SendString("[1] I-Cache Enabled.\r\n");

    /* ---------------------------------------------------------
     * 🥊 Round 1: D-Cache OFF 상태에서 벤치마크
     * --------------------------------------------------------- */
    UART1_SendString("\r\n[2] Running Benchmark (D-Cache OFF)...\r\n");
    Run_Memcopy_Benchmark("[D-Cache OFF]");

    /* ---------------------------------------------------------
     * 🥊 Round 2: D-Cache ON 상태에서 벤치마크
     * --------------------------------------------------------- */
    UART1_SendString("\r\n[3] Enabling D-Cache...\r\n");
    DCache_Enable();
    
    UART1_SendString("[4] Running Benchmark (D-Cache ON)...\r\n");
    Run_Memcopy_Benchmark("[D-Cache ON ]");

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" Benchmark Complete!\r\n");

    while (1) {
        __asm("wfi");
    }
    
    return 0;
}
