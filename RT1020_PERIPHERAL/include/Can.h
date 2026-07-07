#ifndef CAN_H
#define CAN_H

#include <stdint.h>
int Can1_Init(uint8_t presdiv, uint8_t loopback); // initialize(0=OK, 1=timeout). 비트타이밍·LPB는 인자로
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
    uint8_t presdiv;            /* CTRL1[31:24] 프리스케일러 (loopback 데모라 baud 자유) */
    uint8_t loopback;           /* CTRL1[30] loopback enable */
} Can_ConfigType;

void Can_Init(const Can_ConfigType *config);
Std_ReturnType Can_SetControllerMode(uint8_t Controller, Can_ControllerStateType Transition);
Std_ReturnType Can_Write(Can_HwHandleType Hth, const Can_PduType *PduInfo);

#define CAN_MODULE_ID           80u
#define CAN_DEV_ERROR_DETECT    STD_ON

#define CAN_SID_INIT                    0x00u
#define CAN_SID_SETCONTROLLERMODE       0x03u
#define CAN_SID_WRITE                   0x06u

#define CAN_E_PARAM_POINTER     0x01u
#define CAN_E_PARAM_HANDLE      0x02u
#define CAN_E_PARAM_DATA_LENGTH 0x03u
#define CAN_E_UNINIT            0x05u
#define CAN_E_TRANSITION        0x06u

typedef enum
{
    CAN_DRV_UNINIT = 0,
    CAN_DRV_INITIALIZED
} Can_DriverStateType;

#endif
