#ifndef SYSTEM_MIMXRT1020_H_
#define SYSTEM_MIMXRT1020_H_

#include <stdint.h>

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#endif

