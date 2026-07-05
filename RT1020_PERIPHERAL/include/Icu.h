#ifndef ICU_H
#define ICU_H

#include <stdint.h>
#include "Std_Types.h"      /* STD_ON/OFF — 없으면 #if 비교가 항상 참이 되는 함정 */

void Icu1_Init(void);               //ch0 사각파 생성 + ch1 cascade 카운트 시작
uint16_t Icu1_GetEdgeCount(void);   // ch1 CNTR = ch0 누적 Edge 수

// AUTOSAR Wrapping
typedef uint8_t Icu_ChannelType;
typedef uint16_t Icu_EdgeNumberType;
typedef struct
{
    uint8_t dummy;
} Icu_ConfigType;

void Icu_Init(const Icu_ConfigType *ConfigPtr);
Icu_EdgeNumberType Icu_GetEdgeNumbers(Icu_ChannelType Channel);
void Icu_ResetEdgeCount(Icu_ChannelType Channel);

#define ICU_MODULE_ID        122u
#define ICU_DEV_ERROR_DETECT STD_ON

#define ICU_SID_INIT           0x00u
#define ICU_SID_RESETEDGECOUNT 0x0Cu
#define ICU_SID_GETEDGENUMBERS 0x0Fu

#define ICU_E_PARAM_CHANNEL 0x0Bu
#define ICU_E_UNINIT        0x14u

typedef enum
{
    ICU_DRV_UNINIT = 0,
    ICU_DRV_INITIALIZED
} Icu_DriverStateType;

#endif
