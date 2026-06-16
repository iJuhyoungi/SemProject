#ifndef LPSPI_H
#define LPSPI_H

#include <stdint.h>

/* LPSPI1 드라이버 API */
void LPSPI1_Clock_Enable(void);
void LPSPI1_Pin_Init(void);
void LPSPI1_Master_Init(void);
int LPSPI1_Send_Byte(uint8_t b);
int LPSPI1_Send_Buffer(const uint8_t *buf, uint32_t n, uint32_t *peak_out);
void LPSPI1_Send_IRQ(const uint8_t *buf, uint32_t n);
void LPSPI1_Send_DMA(const uint8_t *buf, uint32_t n);
void LPSPI1_Disable(void);

/* ISR <-> main 공유 */
extern volatile uint32_t g_tx_done;
extern volatile uint32_t g_irq_count;
extern volatile uint32_t g_dma_done;
extern volatile uint32_t g_dma_irq_count;

#endif      /* LPSPI_H */
