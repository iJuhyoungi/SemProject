#include "Fee.h"
#include "Det.h"
#include "Fls.h"          /* Fls_Erase / Fls_Write / Fls_GetJobResult 를 아래 계층으로 사용한다 */
#include "crc32.h"        /* 인스턴스 데이터의 CRC 저장용 */
#include "flexspi_ip.h"   /* FlexSPI_ReadData (읽기는 flash 가 busy 가 아니라 동기로 처리) */

/* ================= on-flash 레이아웃 =================
 * Fee 영역은 Fls 허용 영역(0x700000~0x800000) 안의 두 뱅크로 구성된다.
 * 뱅크를 1섹터(4KB)로 잡아, 반복 write 로 GC(뱅크 전환)가 빨리 발동하도록 했다. */
#define FEE_BANK_A_BASE 0x00780000u
#define FEE_BANK_B_BASE 0x00781000u
#define FEE_BANK_SIZE   0x00001000u /* 4KB = 1 sector */

#define FEE_BANK_MAGIC  0x31454546u /* 바이트로 'F','E','E','1' (LE) */
#define FEE_ERASED16    0xFFFFu
#define FEE_ERASED32    0xFFFFFFFFu
#define FEE_WRITE_DONE  0x00000000u /* writeState: 유효 데이터 커밋 */
#define FEE_WRITE_INVAL 0xFFFF0000u /* writeState: 이 블록 무효화 (erased 에서 1->0 로 프로그램 가능) */

#define FEE_BANK_HDR_SIZE 8u
#define FEE_INST_HDR_SIZE 12u
#define FEE_MAX_DATA      16u /* config 의 가장 큰 블록 크기 */

typedef struct
{
    uint32_t magic;
    uint32_t seqNo;
} Fee_BankHeader;

typedef struct
{
    uint16_t blockNumber;
    uint16_t length;
    uint32_t dataCrc;
    uint32_t writeState;
} Fee_InstHeader;

/* write / GC 진행 단계 (각 단계는 Fls 작업 하나) */
typedef enum
{
    W_IDLE = 0,
    W_FMT_ERASE,  /* (활성 뱅크 없을 때) 뱅크 erase */
    W_FMT_HEADER, /* 뱅크 헤더 기록 */
    W_INST,       /* 인스턴스 헤더+데이터 기록 */
    W_MARKER,     /* writeState 마커 기록 (완료 또는 무효) */
    W_FINISH,     /* 일반 write 마무리 */
    W_GC_ERASE,   /* GC: 예비 뱅크 erase */
    W_GC_HEADER,  /* GC: 예비 뱅크 헤더(seqNo+1) 기록 */
    W_GC_STEP,    /* GC: 다음에 복사할 블록을 골라 인스턴스 write 를 발주 */
    W_GC_FINISH   /* GC: 활성 뱅크를 예비로 전환 */
} Fee_WStep;

static struct
{
    MemIf_StatusType    status;
    MemIf_JobResultType result;
    uint32_t            activeBank; /* FEE_BANK_A_BASE / B / 0 */
    uint32_t            activeSeq;
    uint32_t            writePtr;
    uint32_t            index[FEE_MAX_BLOCKS]; /* 블록별 최신 유효 인스턴스 오프셋. 0 = 없음/무효 */

    /* --- job 진행 상태 --- */
    Fee_WStep wstep;
    uint8_t   flsIssued;
    uint8_t   gcMode;   /* 1 = GC 진행 중 */
    int16_t   pos;      /* 지금 쓰는 인스턴스의 블록 위치 */
    uint8_t   curInval; /* 지금 쓰는 인스턴스가 무효화인가 */
    uint32_t  instOff;
    uint32_t  instSize; /* 정렬된 인스턴스 크기 */
    uint32_t  targetBank;
    uint32_t  newSeq;
    uint8_t   bankHdr[FEE_BANK_HDR_SIZE];
    uint8_t   stage[FEE_INST_HDR_SIZE + FEE_MAX_DATA];
    uint16_t  stageLen;
    uint8_t   marker[4];

    /* --- 요청(pending) 보존: GC 로 넘어가도 새 데이터를 유지 --- */
    int16_t pendPos;
    uint8_t pendInval;
    uint8_t pendData[FEE_MAX_DATA];

    /* --- GC 진행 --- */
    uint16_t gcCursor;
    uint32_t gcWritePtr;
    uint32_t gcNewIndex[FEE_MAX_BLOCKS];
    uint8_t  gcReadBuf[FEE_MAX_DATA];
} g_fee = {0};

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int16_t find_block_pos(uint16_t blockNumber)
{
    uint16_t i;
    for (i = 0u; (i < Fee_NumBlocks) && (i < FEE_MAX_BLOCKS); i++)
    {
        if (Fee_BlockConfig[i].blockNumber == blockNumber)
        {
            return (int16_t)i;
        }
    }
    return -1;
}

/* stage 버퍼에 블록 pos 의 인스턴스를 조립한다.
 * withData=1 이면 헤더+데이터, 0 이면 헤더만(무효화 tombstone). */
static void stage_header(int16_t pos, const uint8_t *data, uint8_t withData)
{
    uint16_t sz = Fee_BlockConfig[pos].blockSize;
    uint16_t i;
    uint32_t crc = withData ? CRC32_Compute(data, (uint32_t)sz) : 0u;

    put16(&g_fee.stage[0], Fee_BlockConfig[pos].blockNumber);
    put16(&g_fee.stage[2], sz);
    put32(&g_fee.stage[4], crc);
    put32(&g_fee.stage[8], FEE_ERASED32); /* writeState 는 마커 단계에서 확정한다 */

    if (withData != 0u)
    {
        for (i = 0u; i < sz; i++)
        {
            g_fee.stage[FEE_INST_HDR_SIZE + i] = data[i];
        }
        g_fee.stageLen = (uint16_t)(FEE_INST_HDR_SIZE + sz);
    }
    else
    {
        g_fee.stageLen = FEE_INST_HDR_SIZE; /* 무효화는 데이터 없이 헤더만 쓴다 */
    }

    /* writePtr 전진용 크기는 데이터 유무와 무관하게 헤더+블록크기(정렬)로 예약한다. */
    g_fee.instSize = ((uint32_t)FEE_INST_HDR_SIZE + sz + 3u) & ~3u;
}

static void scan_bank(uint32_t bankBase)
{
    uint32_t       off     = bankBase + FEE_BANK_HDR_SIZE;
    uint32_t       bankEnd = bankBase + FEE_BANK_SIZE;
    Fee_InstHeader ih;

    while ((off + FEE_INST_HDR_SIZE) <= bankEnd)
    {
        if (FlexSPI_ReadData(off, (uint8_t *)&ih, FEE_INST_HDR_SIZE) != FLS_IP_OK)
        {
            break;
        }
        if (ih.blockNumber == FEE_ERASED16)
        {
            break; /* 지워진 헤더 = append 데이터의 끝 */
        }

        {
            int16_t pos = find_block_pos(ih.blockNumber);
            if ((pos >= 0) && (ih.length == Fee_BlockConfig[pos].blockSize))
            {
                if (ih.writeState == FEE_WRITE_DONE)
                {
                    g_fee.index[pos] = off; /* 유효 최신본 */
                }
                else if (ih.writeState == FEE_WRITE_INVAL)
                {
                    g_fee.index[pos] = 0u;  /* 무효화 (나중 것이 이기므로 앞의 유효본을 덮는다) */
                }
                /* 그 밖(0xFFFFFFFF 등)은 torn write → 무시 */
            }
        }

        {
            uint32_t instSize = FEE_INST_HDR_SIZE + ih.length;
            instSize = (instSize + 3u) & ~3u;
            if (instSize <= (FEE_INST_HDR_SIZE - 1u))
            {
                break;
            }
            off += instSize;
        }
    }

    g_fee.writePtr = off;
}

void Fee_Init(void)
{
    uint16_t       i;
    Fee_BankHeader ha;
    Fee_BankHeader hb;
    uint8_t        aValid;
    uint8_t        bValid;

    for (i = 0u; i < FEE_MAX_BLOCKS; i++)
    {
        g_fee.index[i] = 0u;
    }
    g_fee.activeBank = 0u;
    g_fee.activeSeq  = 0u;
    g_fee.writePtr   = 0u;
    g_fee.wstep      = W_IDLE;
    g_fee.flsIssued  = 0u;
    g_fee.gcMode     = 0u;

    (void)FlexSPI_ReadData(FEE_BANK_A_BASE, (uint8_t *)&ha, FEE_BANK_HDR_SIZE);
    (void)FlexSPI_ReadData(FEE_BANK_B_BASE, (uint8_t *)&hb, FEE_BANK_HDR_SIZE);

    aValid = (uint8_t)(ha.magic == FEE_BANK_MAGIC);
    bValid = (uint8_t)(hb.magic == FEE_BANK_MAGIC);

    if (aValid && ((bValid == 0u) || (ha.seqNo >= hb.seqNo)))
    {
        g_fee.activeBank = FEE_BANK_A_BASE;
        g_fee.activeSeq  = ha.seqNo;
    }
    else if (bValid)
    {
        g_fee.activeBank = FEE_BANK_B_BASE;
        g_fee.activeSeq  = hb.seqNo;
    }
    else
    {
        g_fee.activeBank = 0u;
    }

    if (g_fee.activeBank != 0u)
    {
        scan_bank(g_fee.activeBank);
    }

    g_fee.status = MEMIF_IDLE;
    g_fee.result = MEMIF_JOB_OK;
}

Std_ReturnType Fee_Read(uint16_t BlockNumber, uint16_t Offset, uint8_t *Buf, uint16_t Length)
{
    int16_t pos;

    if (g_fee.status == MEMIF_UNINIT)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_READ, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (Buf == 0)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_READ, FEE_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    pos = find_block_pos(BlockNumber);
    if (pos < 0)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_READ, FEE_E_INVALID_BLOCK);
        return E_NOT_OK;
    }
    if (((uint32_t)Offset + (uint32_t)Length) > Fee_BlockConfig[pos].blockSize)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_READ, FEE_E_INVALID_LENGTH);
        return E_NOT_OK;
    }
    if (g_fee.index[pos] == 0u)
    {
        return E_NOT_OK; /* 미기록 또는 무효화된 블록 */
    }
    if (FlexSPI_ReadData(g_fee.index[pos] + FEE_INST_HDR_SIZE + Offset, Buf, Length) != FLS_IP_OK)
    {
        return E_NOT_OK;
    }
    return E_OK;
}

/* pendPos/pendInval/pendData 를 채운 뒤 호출한다. 포맷/일반append/GC 중 무엇으로 갈지 결정한다. */
static void start_job(void)
{
    put32(&g_fee.marker[0], (g_fee.pendInval != 0u) ? FEE_WRITE_INVAL : FEE_WRITE_DONE);
    stage_header(g_fee.pendPos, g_fee.pendData, (uint8_t)(g_fee.pendInval == 0u));
    g_fee.pos      = g_fee.pendPos;
    g_fee.curInval = g_fee.pendInval;
    g_fee.gcMode   = 0u;

    if (g_fee.activeBank == 0u)
    {
        /* 유효 뱅크 없음 → Bank A 포맷 후 append */
        g_fee.targetBank = FEE_BANK_A_BASE;
        g_fee.newSeq     = g_fee.activeSeq + 1u;
        put32(&g_fee.bankHdr[0], FEE_BANK_MAGIC);
        put32(&g_fee.bankHdr[4], g_fee.newSeq);
        g_fee.wstep = W_FMT_ERASE;
    }
    else if ((g_fee.writePtr + g_fee.instSize) <= (g_fee.activeBank + FEE_BANK_SIZE))
    {
        /* 활성 뱅크에 공간이 있다 → 그냥 append */
        g_fee.instOff = g_fee.writePtr;
        g_fee.wstep   = W_INST;
    }
    else
    {
        /* 뱅크가 꽉 찼다 → GC: 예비 뱅크로 옮긴다 */
        g_fee.targetBank = (g_fee.activeBank == FEE_BANK_A_BASE) ? FEE_BANK_B_BASE : FEE_BANK_A_BASE;
        g_fee.newSeq     = g_fee.activeSeq + 1u;
        put32(&g_fee.bankHdr[0], FEE_BANK_MAGIC);
        put32(&g_fee.bankHdr[4], g_fee.newSeq);
        g_fee.gcMode = 1u;
        g_fee.wstep  = W_GC_ERASE;
    }

    g_fee.flsIssued = 0u;
    g_fee.status    = MEMIF_BUSY;
    g_fee.result    = MEMIF_JOB_PENDING;
}

Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *Buf)
{
    int16_t  pos;
    uint16_t sz;
    uint16_t i;

    if (g_fee.status == MEMIF_UNINIT)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_WRITE, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (g_fee.status == MEMIF_BUSY)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_WRITE, FEE_E_BUSY);
        return E_NOT_OK;
    }
    if (Buf == 0)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_WRITE, FEE_E_PARAM_POINTER);
        return E_NOT_OK;
    }
    pos = find_block_pos(BlockNumber);
    if (pos < 0)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_WRITE, FEE_E_INVALID_BLOCK);
        return E_NOT_OK;
    }

    sz = Fee_BlockConfig[pos].blockSize;
    for (i = 0u; i < sz; i++)
    {
        g_fee.pendData[i] = Buf[i];
    }
    g_fee.pendPos   = pos;
    g_fee.pendInval = 0u;
    start_job();
    return E_OK;
}

Std_ReturnType Fee_InvalidateBlock(uint16_t BlockNumber)
{
    int16_t pos;

    if (g_fee.status == MEMIF_UNINIT)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_INVALIDATE, FEE_E_UNINIT);
        return E_NOT_OK;
    }
    if (g_fee.status == MEMIF_BUSY)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_INVALIDATE, FEE_E_BUSY);
        return E_NOT_OK;
    }
    pos = find_block_pos(BlockNumber);
    if (pos < 0)
    {
        Det_ReportError(FEE_MODULE_ID, FEE_INSTANCE_ID, FEE_SID_INVALIDATE, FEE_E_INVALID_BLOCK);
        return E_NOT_OK;
    }

    g_fee.pendPos   = pos;
    g_fee.pendInval = 1u;
    start_job();
    return E_OK;
}

void Fee_MainFunction(void)
{
    uint16_t i;

    if (g_fee.status != MEMIF_BUSY)
    {
        return;
    }

    /* 발주한 Fls 작업이 끝났으면 다음 단계로 넘어간다. */
    if (g_fee.flsIssued != 0u)
    {
        MemIf_JobResultType fr = Fls_GetJobResult();
        if (fr == MEMIF_JOB_PENDING)
        {
            return;
        }
        g_fee.flsIssued = 0u;
        if (fr != MEMIF_JOB_OK)
        {
            g_fee.wstep  = W_IDLE;
            g_fee.gcMode = 0u;
            g_fee.status = MEMIF_IDLE;
            g_fee.result = MEMIF_JOB_FAILED;
            return;
        }

        switch (g_fee.wstep)
        {
        case W_FMT_ERASE:
            g_fee.wstep = W_FMT_HEADER;
            break;
        case W_FMT_HEADER:
            g_fee.activeBank = g_fee.targetBank;
            g_fee.activeSeq  = g_fee.newSeq;
            g_fee.instOff    = g_fee.targetBank + FEE_BANK_HDR_SIZE;
            g_fee.writePtr   = g_fee.instOff;
            g_fee.wstep      = W_INST;
            break;
        case W_INST:
            g_fee.wstep = W_MARKER;
            break;
        case W_MARKER:
            if (g_fee.gcMode != 0u)
            {
                /* GC 복사 한 건 완료 → 다음 블록으로 */
                g_fee.gcWritePtr = g_fee.instOff + g_fee.instSize;
                g_fee.wstep      = W_GC_STEP;
            }
            else
            {
                g_fee.wstep = W_FINISH;
            }
            break;
        case W_GC_ERASE:
            g_fee.wstep = W_GC_HEADER;
            break;
        case W_GC_HEADER:
            g_fee.gcWritePtr = g_fee.targetBank + FEE_BANK_HDR_SIZE;
            g_fee.gcCursor   = 0u;
            for (i = 0u; i < FEE_MAX_BLOCKS; i++)
            {
                g_fee.gcNewIndex[i] = 0u;
            }
            g_fee.wstep = W_GC_STEP;
            break;
        default:
            break;
        }
    }

    /* 현재 단계 발주 */
    switch (g_fee.wstep)
    {
    case W_FMT_ERASE:
    case W_GC_ERASE:
        (void)Fls_Erase(g_fee.targetBank, FEE_BANK_SIZE);
        g_fee.flsIssued = 1u;
        break;

    case W_FMT_HEADER:
    case W_GC_HEADER:
        (void)Fls_Write(g_fee.targetBank, g_fee.bankHdr, FEE_BANK_HDR_SIZE);
        g_fee.flsIssued = 1u;
        break;

    case W_INST:
        (void)Fls_Write(g_fee.instOff, g_fee.stage, g_fee.stageLen);
        g_fee.flsIssued = 1u;
        break;

    case W_MARKER:
        (void)Fls_Write(g_fee.instOff + 8u, g_fee.marker, 4u);
        g_fee.flsIssued = 1u;
        break;

    case W_FINISH:
        g_fee.index[g_fee.pos] = (g_fee.curInval != 0u) ? 0u : g_fee.instOff;
        g_fee.writePtr         = g_fee.instOff + g_fee.instSize;
        g_fee.wstep            = W_IDLE;
        g_fee.status           = MEMIF_IDLE;
        g_fee.result           = MEMIF_JOB_OK;
        break;

    case W_GC_STEP:
    {
        /* 예비 뱅크로 복사할 다음 블록을 고른다. pending 블록은 새 데이터로, 나머지는
         * 옛 뱅크의 최신본을 읽어 옮긴다. 무효화된(또는 무효화 요청) 블록은 버린다. */
        uint8_t issued = 0u;
        while (g_fee.gcCursor < Fee_NumBlocks)
        {
            int16_t p = (int16_t)g_fee.gcCursor;
            g_fee.gcCursor++;

            if (p == g_fee.pendPos)
            {
                if (g_fee.pendInval != 0u)
                {
                    continue; /* 무효화 요청 블록은 새 뱅크로 옮기지 않는다 (드롭) */
                }
                stage_header(p, g_fee.pendData, 1u);
            }
            else if (g_fee.index[p] != 0u)
            {
                uint16_t sz = Fee_BlockConfig[p].blockSize;
                (void)FlexSPI_ReadData(g_fee.index[p] + FEE_INST_HDR_SIZE, g_fee.gcReadBuf, sz);
                stage_header(p, g_fee.gcReadBuf, 1u);
            }
            else
            {
                continue; /* 유효 데이터 없음 → 건너뜀 */
            }

            put32(&g_fee.marker[0], FEE_WRITE_DONE);
            g_fee.pos             = p;
            g_fee.curInval        = 0u;
            g_fee.instOff         = g_fee.gcWritePtr;
            g_fee.gcNewIndex[p]   = g_fee.gcWritePtr;
            g_fee.wstep           = W_INST;
            (void)Fls_Write(g_fee.instOff, g_fee.stage, g_fee.stageLen);
            g_fee.flsIssued = 1u;
            issued          = 1u;
            break;
        }
        if (issued == 0u)
        {
            g_fee.wstep = W_GC_FINISH; /* 다음 호출에서 마무리 */
        }
        break;
    }

    case W_GC_FINISH:
        g_fee.activeBank = g_fee.targetBank;
        g_fee.activeSeq  = g_fee.newSeq;
        for (i = 0u; i < FEE_MAX_BLOCKS; i++)
        {
            g_fee.index[i] = (i < Fee_NumBlocks) ? g_fee.gcNewIndex[i] : 0u;
        }
        g_fee.writePtr = g_fee.gcWritePtr;
        g_fee.gcMode   = 0u;
        g_fee.wstep    = W_IDLE;
        g_fee.status   = MEMIF_IDLE;
        g_fee.result   = MEMIF_JOB_OK;
        break;

    default:
        break;
    }
}

MemIf_StatusType Fee_GetStatus(void)
{
    return g_fee.status;
}

MemIf_JobResultType Fee_GetJobResult(void)
{
    return g_fee.result;
}

/* --- 데모/진단용 (AUTOSAR 표준 아님): 활성 뱅크와 seqNo 를 노출해 GC 발동을 관찰한다. --- */
uint32_t Fee_Dbg_ActiveBank(void)
{
    return g_fee.activeBank;
}
uint32_t Fee_Dbg_ActiveSeq(void)
{
    return g_fee.activeSeq;
}
