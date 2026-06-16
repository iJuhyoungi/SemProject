#include <stdint.h>
#include "uart.h"
#include "rt1020_regs.h"
#include "lpspi.h"
#include "Spi.h"
#include "Dio.h"
#include "Adc.h"

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

static void dwt_init(void)
{
    CM_DEMCR |= (1u << 24); // TRCENA : DWT 모듈 enable
    DWT_CYCCNT = 0u;
    DWT_CTRL |= (1 << 0); // CYCCNTENA : 사이클 count 시작
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[PERI] RT1020 LPSPI — AUTOSAR Spi API\r\n");
    UART1_SendString("=============================\r\n");

    Dio_Init();
    dwt_init();
    Spi_Init(0);

    static const uint8_t buf16[16] = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF};

    static uint8_t big[40];
    for (uint32_t i = 0; i < 40; ++i)
    {
        big[i] = (uint8_t)i;
    }

    /* 동기식 - 전송 내내 main에 묶임 */
    UART1_SendString("[mark] sync...\r\n");
    Spi_WriteIB(0, buf16, 16);
    DWT_CYCCNT = 0u;
    Spi_SyncTransmit(0);
    uint32_t cyc_poll = DWT_CYCCNT;
    UART1_SendString("[mark] sync done\r\n");

    /* 비동기식 IRQ - kick 이후 main은 자유로움 */
    UART1_SendString("[mark] irq...\r\n");
    Spi_SetAsyncMode(SPI_ASYNC_INTERRUPT);
    Spi_WriteIB(0, big, 40);
    DWT_CYCCNT = 0u;
    Spi_AsyncTransmit(0);

    uint32_t cyc_irq = DWT_CYCCNT;
    uint32_t work = 0;
    while (Spi_GetSequenceResult(0) == SPI_SEQ_PENDING)
        work++;
    UART1_SendString("[mark] irq done\r\n");

    /* 비동기 DMA - kick off 이후 main은 묶이지 않고 자유로우며, 인터럽트는 전송 당 1번만 */
    UART1_SendString("[mark] dma...\r\n");
    Spi_SetAsyncMode(SPI_ASYNC_DMA);
    Spi_WriteIB(0, big, 40);
    DWT_CYCCNT = 0u;
    Spi_AsyncTransmit(0);
    uint32_t cyc_dma = DWT_CYCCNT;
    uint32_t dma_work = 0;
    while (Spi_GetSequenceResult(0) == SPI_SEQ_PENDING)
        dma_work++;
    UART1_SendString("[mark] dma done\r\n");

    UART1_SendString("[Spi] poll cyc = ");
    UART1_SendHex32(cyc_poll);
    UART1_SendString("\r\n[Spi] irq  cyc = ");
    UART1_SendHex32(cyc_irq);
    UART1_SendString(" / cnt = ");
    UART1_SendHex32(g_irq_count);
    UART1_SendString("\r\n[Spi] dma  cyc = ");
    UART1_SendHex32(cyc_dma);
    UART1_SendString(" / cnt = ");
    UART1_SendHex32(g_dma_irq_count);
    UART1_SendString("\r\n[Spi] free loops irq/dma = ");
    UART1_SendHex32(work);
    UART1_SendString(" / ");
    UART1_SendHex32(dma_work);
    UART1_SendString("\r\n");

    int adc_fail = ADC1_Init();
    uint32_t vrefsh = ADC1_Read(0x19);          // 0x19(11001) : VREFSH = internal channel, for ADC self-test, hard connected to VRH internally
    uint32_t gs = 0;                            // GS 확인용은 선택
    UART1_SendString("[Adc] init = ");
    UART1_SendString(adc_fail ? "FAIL\r\n" : "OK\r\n");
    UART1_SendString("[Adc] VREFSH R0 = ");
    UART1_SendHex32(vrefsh);
    UART1_SendString("\r\n");

    uint32_t beat = 0;
    while (1)
    {
        Dio_WriteChannel(0, STD_HIGH);
        delay_busy(50000000);
        Dio_WriteChannel(0, STD_LOW);
        delay_busy(50000000);

        UART1_SendString("[PERI] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}
