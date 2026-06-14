#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "rt1020_regs.h"
#include "lpspi.h"

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
    CM_DEMCR |= (1u << 24);         // TRCENA : DWT 모듈 enable
    DWT_CYCCNT = 0u;
    DWT_CTRL |= (1 << 0);           // CYCCNTENA : 사이클 count 시작
}


int main(void)
{
    /* UART1은 startup의 IS_BOOTLOADER 분기가 이미 init 함 — 바로 출력 가능 */
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[PERI] RT1020 peripheral bring-up\r\n");
    UART1_SendString("[PERI] hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    LPSPI1_Clock_Enable(); // LPSPI1 클럭 활성화
    LPSPI1_Pin_Init();     // LPSPI1 핀 설정
    UART1_SendString("[PERI] LPSPI1 pins muxed (ALT1)\r\n");

    LPSPI1_Master_Init();
    UART1_SendString("[PERI] LPSPI1 CFGR1 = ");
    UART1_SendHex32(LPSPI1_CFGR1); /* 기대 0x00000001 */
    UART1_SendString("\r\n[PERI] LPSPI1 CCR   = ");
    UART1_SendHex32(LPSPI1_CCR); /* 기대 0x00000082 (=130) */
    UART1_SendString("\r\n[PERI] LPSPI1 SR    = ");
    UART1_SendHex32(LPSPI1_SR); /* bit0(TDF) set 기대 */
    UART1_SendString("\r\n");

    /* PARAM으로 FIFO 깊이 확인 */
    UART1_SendString("[PERI] PARAM = ");
    UART1_SendHex32(LPSPI1_PARAM);
    UART1_SendString("\r\n");

    static const uint8_t buf[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    UART1_SendString("[PERI] TX 0xA5 ...\r\n");
    int fail = LPSPI1_Send_Byte(0xA5);
    UART1_SendHex32(LPSPI1_FSR);
    UART1_SendString(fail ? "\r\n[PERI] TX TIMEOUT (전송 미완료)\r\n"
                          : "\r\n[PERI] TX OK (TCF set)\r\n");

    uint32_t peak=0;
    UART1_SendString("[PERI] burst 16 bytes ...\r\n");

    fail = LPSPI1_Send_Buffer(buf, 16, &peak);
    UART1_SendString("[PERI] FSR after fill loop = ");
    UART1_SendHex32(LPSPI1_FSR);
    UART1_SendString("\r\n");


    UART1_SendString("[PERI] FIFO peak during burst = ");
    UART1_SendHex32(peak);
    UART1_SendString(fail ? "\r\n[PERI] BURST TIMEOUT\r\n"
                          : "\r\n[PERI] BURST OK (TCF set)\r\n");

    UART1_SendString("[PERI] LPSPI1 VERID = ");
    UART1_SendHex32(LPSPI1_VERID);
    UART1_SendString("\r\n");

    dwt_init();

    static uint8_t big[40];
    for(uint32_t i=0;i<40;++i)big[i]=(uint8_t)i;

    /*polling방식*/
    DWT_CYCCNT=0u;
    LPSPI1_Send_Buffer(big,16,&(uint32_t){0});      // 16개(tight버전 n<=16)
    uint32_t cyc_poll=DWT_CYCCNT;

    /*IRQ방식 - Kick-off만 하고 즉시 리턴*/
    DWT_CYCCNT=0u;
    LPSPI1_Send_IRQ(big,40);                        // 40개, 백그라운드 전송
    uint32_t cyc_irq_kick=DWT_CYCCNT;               // main이 묶여있는 시간은 kick-off뿐

    /*그 사이 main운 자유로우며, 일부러 다른 task를 하며 전송이 끝나는지 확인*/
    uint32_t work=0;
    while(!g_tx_done){                              // 전송과 병렬로 루프를 수행한 count
        work++;
    }
    UART1_SendString("[PERI] poll cycles = ");
    UART1_SendHex32(cyc_poll);
    UART1_SendString("\r\n[PERI] irq kick cyc  = ");
    UART1_SendHex32(cyc_irq_kick);
    UART1_SendString("\r\n[PERI] irq count     = ");
    UART1_SendHex32(g_irq_count);
    UART1_SendString("\r\n[PERI] main work loops= ");
    UART1_SendHex32(work);
    UART1_SendString("\r\n");

    UART1_SendString("[PERI] DMA send 40 bytes ...\r\n");
    DWT_CYCCNT=0u;
    LPSPI1_Send_DMA(big, 40);             /* big[40] 재사용 */
    uint32_t cyc_dma_kick=DWT_CYCCNT;

    uint32_t dma_work=0;
    while(!g_dma_done){
        dma_work++;
    }

      UART1_SendString("[PERI] dma kick cyc  = "); UART1_SendHex32(cyc_dma_kick);
      UART1_SendString("\r\n[PERI] dma irq count= "); UART1_SendHex32(g_dma_irq_count);
      UART1_SendString("\r\n[PERI] dma work loops="); UART1_SendHex32(dma_work);
      UART1_SendString("\r\n");

    /* DMA 메이저 루프 완료 대기 (CSR[DONE]=bit7) */
    uint32_t to1 = 2000000u;
    while (!(EDMA_TCD0_CSR & (1u << 7)) && --to1) { }

    /* 마지막 byte 가 FIFO 에서 다 빠질 때까지 (TCF) */
    uint32_t to2 = 2000000u;
    while (!(LPSPI1_SR & (1u << 10)) && --to2) { }

    UART1_SendString("[PERI] DMA CSR = ");
    UART1_SendHex32(EDMA_TCD0_CSR);        /* DONE(bit7) set 기대 → 0x...80 류 */
    UART1_SendString((to1 && to2) ? "\r\n[PERI] DMA OK\r\n"
                                : "\r\n[PERI] DMA TIMEOUT\r\n");

    uint32_t beat = 0;
    while (1)
    {
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