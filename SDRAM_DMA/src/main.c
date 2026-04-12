#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "led.h"
#include "semc.h"
#include "cache.h"
#include "dma.h"
#include "rt1020_regs.h"

/* 🚀 섹션 지정자 삭제! 이제 일반 SDRAM(.bss)에 할당되며 캐시의 영향을 100% 받습니다.
 * 단, 캐시 라인 오염을 막기 위해 32바이트 정렬만 유지합니다. */
uint8_t dma_tx_buffer[4096] __attribute__((aligned(32)));
uint8_t dma_rx_buffer[4096] __attribute__((aligned(32)));

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
    DMA_Init(); /* 🚀 DMA 엔진 초기화 */

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" Phase 1.5: DMA Coherency Test\r\n");
    UART1_SendString("=================================\r\n");

    /* D-Cache를 무조건 켜고 시작 (DMA와 캐시가 싸우는지 확인하기 위함) */
    ICache_Enable();
    DCache_Enable();
    UART1_SendString("[1] Core I/D Cache Enabled.\r\n");

    /* 1. 송신 버퍼에 테스트 데이터 꽉꽉 채우기 (CPU가 기록) */
    for (uint32_t i = 0; i < 4096; i++) {
        dma_tx_buffer[i] = (uint8_t)(i & 0xFF);
        dma_rx_buffer[i] = 0; /* 수신 버퍼는 초기화 */
    }
    UART1_SendString("[2] TX Buffer Prepared by CPU.\r\n");

    DCache_CleanByAddr(dma_tx_buffer, 4096);
    
    /* 2. DMA를 시켜서 TX에서 RX로 4096바이트 하드웨어 복사 지시! */
    UART1_SendString("[3] Triggering eDMA Memory-to-Memory Transfer...\r\n");
    DMA_MemCopy(dma_rx_buffer, dma_tx_buffer, 4096);
    UART1_SendString("[4] DMA_MemCopy returned.\r\n");

/* 🚀 3. 동적 동기화 (RX): CPU가 읽기 전에 RX 버퍼의 옛날 캐시를 박살냅니다! */
    UART1_SendString("[5] Invalidating RX Cache to fetch new SDRAM data...\r\n");
    DCache_InvalidateByAddr(dma_rx_buffer, 4096);

    /* 4. 복사된 데이터가 완벽하게 일치하는지 검증 */
    uint8_t success = 1;
    for (uint32_t i = 0; i < 4096; i++) {
        if (dma_rx_buffer[i] != (uint8_t)(i & 0xFF)) {
            success = 0;
            break;
        }
    }

    if (success) {
        UART1_SendString(" -> 🟢 DMA Non-Cacheable Buffer Test PASS!\r\n");
    } else {
        UART1_SendString(" -> 🔴 DMA Test FAILED! (Cache Coherency Issue)\r\n");
    }

    while (1) {
        __asm("wfi");
    }
    return 0;
}
