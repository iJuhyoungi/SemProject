#ifndef PWM_H
#define PWM_H

#include <stdint.h>

// register level
void Pwm1_Init(void);
uint16_t Pwm1_GetCount(void);

// AUTOSAR Pwm Wrapping
typedef uint8_t Pwm_ChannelType;
typedef struct {
    uint8_t dummy;
}Pwm_ConfigType;        // level1: minimum

void Pwm_Init(const Pwm_ConfigType *ConfigPtr);
void Pwm_SetDutyCycle(Pwm_ChannelType Channel, uint16_t DutyCycle);         // 0x0000~0x8000

#endif
