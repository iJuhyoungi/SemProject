#ifndef _DMA_H_
#define _DMA_H_

#include <stdint.h>

void DMA_Init(void);
void DMA_MemCopy(void *dst, void *src, uint32_t length);

#endif