#ifndef GPT_H
#define GPT_H

#include <stdint.h>
#include "Std_Types.h"

/* --- register층 (기존) --- */
void Gpt1_Init(void);
uint32_t Gpt1_GetCount(void);
uint32_t Gpt1_GetTick(void);

/* --- AUTOSAR Wrapping --- */
typedef uint8_t  Gpt_ChannelType;
typedef uint32_t Gpt_ValueType;
typedef struct { uint8_t dummy; } Gpt_ConfigType;

void          Gpt_Init(const Gpt_ConfigType *ConfigPtr);
Gpt_ValueType Gpt_GetTimeElapsed(Gpt_ChannelType Channel);
Gpt_ValueType Gpt_GetTimeRemaining(Gpt_ChannelType Channel);
void          Gpt_EnableNotification(Gpt_ChannelType Channel);
void          Gpt_DisableNotification(Gpt_ChannelType Channel);

#define GPT_MODULE_ID        100u
#define GPT_DEV_ERROR_DETECT STD_ON

#define GPT_SID_INIT            0x01u
#define GPT_SID_GETTIMEELAPSED  0x03u
#define GPT_SID_GETTIMEREMAIN   0x04u
#define GPT_SID_ENABLENOTIF     0x07u
#define GPT_SID_DISABLENOTIF    0x08u

#define GPT_E_UNINIT        0x0Au
#define GPT_E_PARAM_CHANNEL 0x14u
typedef enum
{
    GPT_DRV_UNINIT = 0,
    GPT_DRV_INITIALIZED
} Gpt_DriverStateType;

#endif
