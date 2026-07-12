#include <stdint.h>
#include "uart.h"
#include "rt1020_regs.h"
#include "lpspi.h"
#include "Spi.h"
#include "Dio.h"
#include "Adc.h"
#include "Pwm.h"
#include "Can.h"
#include "Gpt.h"
#include "Wdg.h"
#include "Icu.h"
#include "Mcu.h"
#include "Cfg.h"
#include "WdgM.h"
#include "Port.h"

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
    Spi_Init(&Spi_Config);

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

    UART1_SendString("[Det-test] expect 3 reports:\r\n");
    Adc_ValueGroupType adc_buf[1];
    (void)Adc_ReadGroup(0, adc_buf);
    Adc_Init(&Adc_Config);
    (void)Adc_ReadGroup(7, adc_buf);
    (void)Adc_ReadGroup(0, (Adc_ValueGroupType *)0);
    
    Std_ReturnType adc_r=Adc_ReadGroup(0,adc_buf);
    UART1_SendString("[Adc] ReadGroup = ");
    UART1_SendString((adc_r == E_OK) ? "OK\r\n" : "FAIL\r\n");
    UART1_SendString("[Adc] VREFSH R0 = ");
    UART1_SendHex32(adc_buf[0]);
    UART1_SendString("\r\n");

    Pwm_Init(&Pwm_Config);
    Pwm_SetDutyCycle(0,0x2000);
    UART1_SendString("[Pwm] duty 25% set, VAL3 = ");
    UART1_SendHex32(FLEXPWM1_SM0VAL3);
    UART1_SendString("\r\n");

    uint16_t c1=Pwm1_GetCount();
    delay_busy(100000);
    uint16_t c2=Pwm1_GetCount();

    UART1_SendString("[Pwm] CNT1 = ");
    UART1_SendHex32(c1);
    UART1_SendString("\r\n[Pwm] CNT2 = ");
    UART1_SendHex32(c2);
    UART1_SendString(c1 != c2 ? "\r\n[Pwm] running (counter advancing)\r\n"
                                : "\r\n[Pwm] NOT running\r\n");

    Can_Init(&Can_Config);
    UART1_SendString("[Can] init = OK\r\n");
    UART1_SendString("[Can] MCR  = ");
    UART1_SendHex32(FLEXCAN1_MCR);
    UART1_SendString("\r\n");

    int lb=Can1_LoopbackTest();
    UART1_SendString("[Can] loopback = ");
    if      (lb == 0) UART1_SendString("OK (TX -> RX data match)\r\n");
    else if (lb == 1) UART1_SendString("FAIL: no RX (timeout)\r\n");
    else if (lb == 2) UART1_SendString("FAIL: CODE not FULL\r\n");
    else              UART1_SendString("FAIL: data mismatch\r\n");

    Can_SetControllerMode(0, CAN_CS_STARTED);
    uint8_t txdata[8]={0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
    Can_PduType pdu={.swPduHandle=0, .length=8, .id=0x123, .sdu=txdata};
    Std_ReturnType w=Can_Write(0,&pdu);
    UART1_SendString("[Can] write(HOH=0) = ");
    UART1_SendString(w == E_OK ? "OK\r\n" : "FAIL\r\n");

    /* DET negative test: 9바이트 PDU 는 classic CAN 한계(8) 초과 -> PARAM_DATA_LENGTH 신고 기대 */
    uint8_t big9[9]={0};
    Can_PduType bad={.swPduHandle=0, .length=9, .id=0x123, .sdu=big9};
    (void)Can_Write(0,&bad);

    Gpt_Init(&Gpt_Config);
    uint32_t g1=Gpt1_GetCount();
    delay_busy(100000);
    uint32_t g2=Gpt1_GetCount();
    UART1_SendString("[Gpt] CNT advancing = ");
    UART1_SendString(g2!=g1?"YES\r\n":"NO\r\n");
    UART1_SendString("[Gpt] elapsed   = ");
    UART1_SendHex32(Gpt_GetTimeElapsed(0));
    UART1_SendString("\r\n[Gpt] remaining = ");
    UART1_SendHex32(Gpt_GetTimeRemaining(0));
    UART1_SendString("\r\n");

    Mcu_ResetType rst = Mcu_GetResetReason();
    int by_wdg = (rst == MCU_WATCHDOG_RESET);   /* 비트 지식이 표준 enum 으로 승격 */
    UART1_SendString("[Mcu] reset SRSR = ");
    UART1_SendHex32(Mcu_GetResetRawValue());
    UART1_SendString(by_wdg ? "  <- watchdog reset!\r\n" : "\r\n");
    Mcu_ClearResetReason();

    Icu_Init(&Icu_Config);
    Port_RoutePwmTrigToQtmr();
    uint16_t e1=Icu_GetEdgeNumbers(0);
    delay_busy(100000);
    uint16_t e2=Icu_GetEdgeNumbers(0);
    UART1_SendString("[Icu] CNTR advancing = ");
    UART1_SendString(e2 != e1 ? "YES\r\n" : "NO\r\n");


    Wdg_Init(&Wdg_Config);
    UART1_SendString("[Wdg] CS    = ");
    UART1_SendHex32(Wdg1_GetCS());
    UART1_SendString("\r\n");
    UART1_SendString("[Wdg] TOVAL = ");
    UART1_SendHex32(Wdg1_GetTOVAL());
    UART1_SendString("\r\n"); /* 0x000FFFFF 기대 */
    UART1_SendString("[Wdg] CNT0  = ");
    UART1_SendHex32(Wdg1_GetCNT());
    UART1_SendString("\r\n");

    WdgM_Init(&WdgM_Config);

    uint32_t beat = 0;
    while (1)
    {
        int alive = by_wdg || (beat < 5); 

        if(alive) WdgM_CheckpointReached(0, 0);

        UART1_SendString("[Icu] edge count = ");
        UART1_SendHex32(Icu_GetEdgeNumbers(0));
        UART1_SendString("\r\n");
        UART1_SendString("[Icu] pwm edges  = ");
        UART1_SendHex32(Icu_GetEdgeNumbers(1));
        UART1_SendString("\r\n");

        Dio_WriteChannel(0, STD_HIGH);
        delay_busy(20000000);

        if(alive) WdgM_CheckpointReached(0, 0);

        Dio_WriteChannel(0, STD_LOW);
        delay_busy(20000000);

        UART1_SendString("[PERI] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
        UART1_SendString("[Gpt] tick = ");
        UART1_SendHex32(Gpt1_GetTick());
        UART1_SendString("\r\n");

        WdgM_GlobalStatusType wdgm_st;
        if(WdgM_GetGlobalStatus(&wdgm_st)==E_OK){
            UART1_SendString("[WdgM] status = ");
            UART1_SendHex32(wdgm_st);
            UART1_SendString("\r\n");
        }
    }

    return 0;
}
