#ifndef ADC_H
#define ADC_H

#include <stdint.h>

int         ADC1_Init(void);                    // 클럭 + CFG + calibration. 0=OK, 1=timeout
uint32_t    ADC1_Read(uint8_t channel);


// AUTOSAR Wrapping
#include "Std_Types.h"
typedef uint8_t Adc_GroupType;
typedef uint16_t Adc_ValueGroupType;
typedef struct
{
    uint8_t dummy;
} Adc_ConfigType;

typedef enum
{
    ADC_IDLE = 0,
    ADC_BUSY,
    ADC_COMPLETED
} Adc_StatusType;

void Adc_Init(const Adc_ConfigType *ConfigPtr);
Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr);
Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group);

#endif
