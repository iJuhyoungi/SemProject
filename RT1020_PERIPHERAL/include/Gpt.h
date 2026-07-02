#ifndef GPT_H
#define GPT_H

#include <stdint.h>

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

#endif
