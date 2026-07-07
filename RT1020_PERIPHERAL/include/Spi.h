#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include "Std_Types.h"

/* AUTOSAR Spi Standard Type */
typedef enum
{
    SPI_UNINIT = 0,
    SPI_IDLE,
    SPI_BUSY
} Spi_StatusType;

typedef enum
{
    SPI_SEQ_OK = 0,
    SPI_SEQ_PENDING,
    SPI_SEQ_FAILED
} Spi_SeqResultType;

typedef uint8_t Spi_ChannelType;
typedef uint8_t Spi_SequenceType;

/* async 전송 방식 - IRQ/DMA 구현 선택 */
typedef enum
{
    SPI_ASYNC_INTERRUPT = 0,
    SPI_ASYNC_DMA
} Spi_AsyncModeType;

typedef struct
{
    uint32_t sckdiv;
} Spi_ConfigType;

/* API */
void              Spi_Init(const Spi_ConfigType *ConfigPtr);
Std_ReturnType    Spi_DeInit(void);
Std_ReturnType    Spi_WriteIB(Spi_ChannelType Channel, const uint8_t *src, uint16_t len);
Std_ReturnType    Spi_SyncTransmit(Spi_SequenceType Sequence);
Std_ReturnType    Spi_AsyncTransmit(Spi_SequenceType Sequence);
void              Spi_SetAsyncMode(Spi_AsyncModeType Mode);
Spi_StatusType    Spi_GetStatus(void);
Spi_SeqResultType Spi_GetSequenceResult(Spi_SequenceType Sequence);

#define SPI_MODULE_ID           83u
#define SPI_DEV_ERROR_DETECT    STD_ON

#define SPI_SID_INIT            0x00u
#define SPI_SID_WRITEIB         0x02u
#define SPI_SID_ASYNCTRANSMIT   0x03u
#define SPI_SID_GETSEQRESULT    0x08u
#define SPI_SID_SYNCTRANSMIT    0x0Au
#define SPI_SID_SETASYNCMODE    0x0Du

#define SPI_E_PARAM_CHANNEL     0x0Au
#define SPI_E_PARAM_LENGTH      0x0Du
#define SPI_E_PARAM_POINTER     0x10u
#define SPI_E_UNINIT            0x1Au
#define SPI_E_SEQ_PENDING       0x2Au
#define SPI_E_ALREADY_INITIALIZED 0x4Au     /* Spi 는 0x4A — 모듈마다 대역이 다름 */


#endif /* SPI_H */
