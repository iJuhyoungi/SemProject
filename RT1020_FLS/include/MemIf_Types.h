#ifndef MEMIF_TYPES_H
#define MEMIF_TYPES_H

/* Fls·Fee 가 공유하는 메모리 스택 공통 타입 (AUTOSAR MemIf) */
typedef enum
{
    MEMIF_UNINIT = 0,   /* 아직 Init 안 됨 */
    MEMIF_IDLE,         /* 한가함 — 새 job 받을 수 있음 */
    MEMIF_BUSY          /* job 진행 중 */
} MemIf_StatusType;

typedef enum
{
    MEMIF_JOB_OK = 0,       /* 마지막 job 성공 */
    MEMIF_JOB_FAILED,       /* 하드웨어 실패 */
    MEMIF_JOB_PENDING,      /* 진행 중 */
    MEMIF_JOB_CANCELED,
    MEMIF_BLOCK_INCONSISTENT,
    MEMIF_BLOCK_INVALID
} MemIf_JobResultType;


#endif
