#include <stdint.h>
#include "Spi.h"
#include "lpspi.h"
#include "Mcu.h"
#include "Port.h"

/* --- 내부 상태 --- */
static Spi_StatusType s_status = SPI_UNINIT;
static Spi_AsyncModeType s_async_mode = SPI_ASYNC_INTERRUPT;

/* 내부 송신 버퍼 (Spi_WriteIB 로 적재 → Transmit 으로 송신) */
#define SPI_IB_SIZE 64u
static uint8_t s_ib_buf[SPI_IB_SIZE];
static uint16_t s_ib_len;

void Spi_Init(const Spi_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;

    Mcu_InitClock();                // Clock gate
    Port_Init();                    // 핀 mux (Port)
    LPSPI1_Master_Init();           // LPSPI 설정

    s_ib_len = 0;
    s_status = SPI_IDLE;
}

Std_ReturnType Spi_DeInit(void)
{
    LPSPI1_Disable();
    s_status = SPI_UNINIT;
    return E_OK;
}

Std_ReturnType Spi_WriteIB(Spi_ChannelType Channel, const uint8_t *src, uint16_t len)
{
    (void)Channel;
    if (src == 0 || len > SPI_IB_SIZE)
    {
        return E_NOT_OK;
    }

    for (uint16_t i = 0; i < len; ++i)
    {
        s_ib_buf[i] = src[i];
    }
    s_ib_len = len;

    return E_OK;
}

/* blocking 송신 */
Std_ReturnType Spi_SyncTransmit(Spi_SequenceType Sequence)
{
    (void)Sequence;
    if (s_status == SPI_UNINIT || s_ib_len == 0 || s_ib_len > 16u)
        return E_NOT_OK;

    s_status = SPI_BUSY;
    int fail = LPSPI1_Send_Buffer(s_ib_buf, s_ib_len, 0); /* peak_out=NULL */
    s_status = SPI_IDLE;
    return fail ? E_NOT_OK : E_OK;
}

/* non-blocking 송신 */
Std_ReturnType Spi_AsyncTransmit(Spi_SequenceType Sequence)
{
    (void)Sequence;
    if (s_status == SPI_UNINIT || s_ib_len == 0)
        return E_NOT_OK;

    s_status = SPI_BUSY;
    if (s_async_mode == SPI_ASYNC_DMA)
    {
        LPSPI1_Send_DMA(s_ib_buf, s_ib_len);
    }
    else
    {
        LPSPI1_Send_IRQ(s_ib_buf, s_ib_len);
    }
    return E_OK;
}

void Spi_SetAsyncMode(Spi_AsyncModeType Mode)
{
    s_async_mode = Mode;
}

Spi_StatusType Spi_GetStatus(void)
{
    return s_status;
}

Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence)
{
    (void)Sequence;

    uint32_t done = (s_async_mode == SPI_ASYNC_DMA) ? g_dma_done : g_tx_done;
    if (done)
    {
        s_status = SPI_IDLE;
        return SPI_SEQ_OK;
    }
    return SPI_SEQ_PENDING;
}
