#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include "Std_Types.h"

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

#define PWM_MODULE_ID           121u
#define PWM_DEV_ERROR_DETECT    STD_ON
#define PWM_SID_INIT            0x00u
#define PWM_SID_SETDUTYCYCLE    0x02u

#define PWM_E_UNINIT            0x11u
#define PWM_E_PARAM_CHANNEL     0x12u

typedef enum
{
    PWM_DRV_UNINIT = 0,
    PWM_DRV_INITIALIZED
} Pwm_DriverStateType;

#endif
