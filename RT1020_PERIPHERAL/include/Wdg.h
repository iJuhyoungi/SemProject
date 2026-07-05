#ifndef WDG_H
#define WDG_H

#include <stdint.h>

void Wdg1_Init(uint32_t timeout_ticks);         // unlock + CS/TOVAL설정 + EN
void Wdg1_Refresh(void);                        // 0xB480A602 -> CNT
uint32_t Wdg1_GetCS(void);                      // 진단용 CS readback
uint32_t Wdg_GetResetCause(void);               // SRC_SRSR
void Wdg_ClearResetCause(void);                 // SRSR W1C
uint32_t Wdg1_GetCNT(void);                     // 현재 카운터
uint32_t Wdg1_GetTOVAL(void);                   // TOVAL 확인

#include "Std_Types.h"
typedef uint8_t Wdg_ModeType;
#define WDGIF_OFF_MODE 0u
#define WDGIF_SLOW_MODE 1u
#define WDGIF_FAST_MODE 2u
typedef struct
{
    uint8_t dummy;
} Wdg_ConfigType;

void Wdg_Init(const Wdg_ConfigType *ConfigPtr);
Std_ReturnType Wdg_SetMode(Wdg_ModeType Mode);
void Wdg_SetTriggerCondition(uint16_t timeout);

  #define WDG_MODULE_ID        102u
  #define WDG_DEV_ERROR_DETECT STD_ON

  #define WDG_SID_INIT             0x00u
  #define WDG_SID_SETMODE          0x01u
  #define WDG_SID_SETTRIGGERCOND   0x03u

  #define WDG_E_DRIVER_STATE 0x10u
  #define WDG_E_PARAM_MODE   0x11u

typedef enum
{
    WDG_DRV_UNINIT = 0,
    WDG_DRV_INITIALIZED
} Wdg_DriverStateType;

#endif
