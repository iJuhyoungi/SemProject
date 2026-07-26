#ifndef FEE_H
#define FEE_H

#include <stdint.h>
#include "Std_Types.h"
#include "MemIf_Types.h"

/* 블록 config. 상위(NvM)는 블록 번호로만 접근하고, 크기는 여기서 정의한다. */
typedef struct
{
    uint16_t blockNumber; /* 논리 블록 번호 (1 부터) */
    uint16_t blockSize;   /* 데이터 크기(바이트) */
} Fee_BlockConfigType;

extern const Fee_BlockConfigType Fee_BlockConfig[];
extern const uint16_t            Fee_NumBlocks;

/* config 에 담을 수 있는 최대 블록 수 (RAM 인덱스 배열 크기) */
#define FEE_MAX_BLOCKS 8u

void                Fee_Init(void);
Std_ReturnType      Fee_Read(uint16_t BlockNumber, uint16_t Offset, uint8_t *Buf, uint16_t Length);
Std_ReturnType      Fee_Write(uint16_t BlockNumber, const uint8_t *Buf);
Std_ReturnType      Fee_InvalidateBlock(uint16_t BlockNumber);
void                Fee_MainFunction(void);
MemIf_StatusType    Fee_GetStatus(void);
MemIf_JobResultType Fee_GetJobResult(void);

/* 데모/진단용 (AUTOSAR 표준 아님): 활성 뱅크와 seqNo 를 노출해 GC(뱅크 전환)를 관찰한다. */
uint32_t            Fee_Dbg_ActiveBank(void);
uint32_t            Fee_Dbg_ActiveSeq(void);

/* ---- DET 식별자 ---- */
#define FEE_MODULE_ID          21u   /* AUTOSAR Fee 모듈 ID */
#define FEE_INSTANCE_ID        0u

#define FEE_SID_INIT           0x00u
#define FEE_SID_READ           0x02u
#define FEE_SID_WRITE          0x03u
#define FEE_SID_INVALIDATE     0x05u

#define FEE_E_UNINIT           0x01u  /* 초기화 전에 호출했다 */
#define FEE_E_INVALID_BLOCK    0x02u  /* config 에 없는 블록 번호다 */
#define FEE_E_PARAM_POINTER    0x03u  /* 버퍼 포인터가 NULL 이다 */
#define FEE_E_INVALID_LENGTH   0x04u  /* offset+length 가 블록 크기를 넘는다 */
#define FEE_E_BUSY             0x05u  /* 이미 job 이 진행 중인데 새 요청이 왔다 */

#endif /* FEE_H */
