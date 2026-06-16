#ifndef ADC_H
#define ADC_H

#include <stdint.h>

int         ADC1_Init(void);                    // 클럭 + CFG + calibration. 0=OK, 1=timeout
uint32_t    ADC1_Read(uint8_t channel);

#endif
