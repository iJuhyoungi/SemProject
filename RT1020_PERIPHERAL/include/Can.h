#ifndef CAN_H
#define CAN_H

#include <stdint.h>
int Can1_Init(void); // loopback 모드 initialize(0=OK, 1=timeout)
int Can1_LoopbackTest(void);            // 프레임 1개 TX -> loopback RX -> 데이터 일치 확인

#include "Std_Types.h"

// AUTOSAR Wrapping
typedef uint32_t Can_IdType;
typedef uint8_t Can_HwHandleType;
typedef uint16_t PduIdType;
typedef struct
{
    PduIdType swPduHandle;  // software handle
    uint8_t length;         // DLC
    Can_IdType id;          // CAN ID
    uint8_t *sdu;           // data pointer
} Can_PduType;

typedef enum
{
    CAN_CS_STARTED,
    CAN_CS_STOPPED,
    CAN_CS_SLEEP
} Can_ControllerStateType;

typedef struct
{
    uint8_t dummy;
} Can_ConfigType;

void Can_Init(const Can_ConfigType *config);
Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

#endif
