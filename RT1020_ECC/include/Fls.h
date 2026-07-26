#ifndef FLS_H
#define FLS_H

#include <stdint.h>
#include "Std_Types.h"
#include "MemIf_Types.h"

typedef uint32_t Fls_AddressType;   /* flash 기준 오프셋 (0-base, AHB 아님) */
typedef uint32_t Fls_LengthType;

/* post-build config: 접근 허용 영역과 기하 구조 */
typedef struct
{
    Fls_AddressType baseAddress;    /* 허용 영역 시작 (예: 0x00700000) */
    Fls_LengthType  totalSize;      /* 허용 영역 크기 (예: 0x00100000) */
    Fls_LengthType  sectorSize;     /* 4096 */
    Fls_LengthType  pageSize;       /* 256 */
    Fls_LengthType  maxWriteChunk;  /* MainFunction 한 번의 최대 write 바이트 (<=32) */
} Fls_ConfigType;

/* Fls_Cfg.c 에 정의된 실제 config 테이블을 main 등에서 참조할 수 있게 공개한다. */
extern const Fls_ConfigType Fls_Config;

void                Fls_Init(const Fls_ConfigType *ConfigPtr);
Std_ReturnType      Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length);
Std_ReturnType      Fls_Write(Fls_AddressType TargetAddress, const uint8_t *SourceAddressPtr, Fls_LengthType Length);
Std_ReturnType      Fls_Read (Fls_AddressType SourceAddress, uint8_t *TargetAddressPtr, Fls_LengthType Length);
void                Fls_MainFunction(void);
MemIf_StatusType    Fls_GetStatus(void);
MemIf_JobResultType Fls_GetJobResult(void);

/* ---- DET (peripheral 챕터 관례) ---- */
#define FLS_MODULE_ID           92u     /* AUTOSAR Fls 모듈 ID */
#define FLS_INSTANCE_ID         0u
#define FLS_DEV_ERROR_DETECT    STD_ON

#define FLS_SID_INIT            0x00u
#define FLS_SID_ERASE           0x01u
#define FLS_SID_WRITE           0x02u
#define FLS_SID_READ            0x07u
#define FLS_SID_MAINFUNCTION    0x06u

#define FLS_E_UNINIT            0x01u   /* Init 전에 호출 */
#define FLS_E_BUSY              0x02u   /* 이미 job 진행 중인데 새 요청 */
#define FLS_E_PARAM_ADDRESS     0x03u   /* 허용 영역 밖 / 정렬 안 됨 */
#define FLS_E_PARAM_LENGTH      0x04u   /* 길이 정렬 안 됨 / 0 */
#define FLS_E_PARAM_DATA        0x05u   /* NULL 포인터 */

#endif /* FLS_H */
