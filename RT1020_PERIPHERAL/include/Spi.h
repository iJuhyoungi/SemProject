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


#endif /* SPI_H */