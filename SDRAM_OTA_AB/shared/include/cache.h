#ifndef _CACHE_H_
#define _CACHE_H_

void ICache_Enable(void);
void DCache_Enable(void);
void DCache_Disable(void);
void DCache_CleanByAddr(void *addr, uint32_t size);
void DCache_InvalidateByAddr(void *addr, uint32_t size);

#endif
