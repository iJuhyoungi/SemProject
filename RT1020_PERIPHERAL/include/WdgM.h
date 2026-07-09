#ifndef WDGM_H
#define WDGM_H

#include <stdint.h>
#include "Std_Types.h"

typedef uint16_t WdgM_SupervisedEntityIdType;
typedef uint16_t WdgM_CheckpointIdType;

typedef enum {
    WDGM_LOCAL_STATUS_OK = 0,
    WDGM_LOCAL_STATUS_FAILED,
    WDGM_LOCAL_STATUS_EXPIRED,
    WDGM_LOCAL_STATUS_DEACTIVATED,
} WdgM_LocalStatusType;

typedef WdgM_LocalStatusType WdgM_GlobalStatusType;

typedef struct {
    uint8_t expected_min;
    uint8_t expected_max;
    uint8_t failed_tolerance;
}WdgM_ConfigType;

// API
void WdgM_Init(const WdgM_ConfigType* ConfigPtr);
Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, WdgM_CheckpointIdType CheckpointID);

void WdgM_MainFunction(void);
Std_ReturnType WdgM_GetGlobalStatus(WdgM_GlobalStatusType* Status);

/* --- DET --- */
#define WDGM_MODULE_ID        13u
#define WDGM_DEV_ERROR_DETECT STD_ON

#define WDGM_SID_INIT              0x00u
#define WDGM_SID_MAINFUNCTION      0x08u
#define WDGM_SID_GETGLOBALSTATUS   0x0Du
#define WDGM_SID_CHECKPOINTREACHED 0x0Eu

#define WDGM_E_NO_INIT      0x10u
#define WDGM_E_PARAM_CONFIG 0x11u
#define WDGM_E_PARAM_SEID   0x12u
#define WDGM_E_PARAM_POINTER 0x13u

#endif // WDGM_H
