#include "Fee.h"
#include "Det.h"
#include "Fls.h"          /* Fls_Erase / Fls_Write / Fls_GetJobResult 를 아래 계층으로 사용한다 */
#include "crc32.h"        /* 인스턴스 데이터의 CRC 저장용 */
#include "flexspi_ip.h"   /* FlexSPI_ReadData (읽기는 flash 가 busy 가 아니라 동기로 처리) */

/* ================= on-flash 레이아웃 =================
 * Fee 영역은 Fls 허용 영역(0x700000~0x800000) 안의 두 뱅크로 구성된다.
 * F-4/F-5 실험 섹터(0x7FF000)와 겹치지 않도록 앞쪽에 배치했다. */
#define FEE_BANK_A_BASE 0x00780000u
#define FEE_BANK_B_BASE 0x00788000u
#define FEE_BANK_SIZE   0x00008000u /* 32KB = 8 sector */

#define FEE_BANK_MAGIC  0x31454546u /* 바이트로 'F','E','E','1' (LE) */
#define FEE_ERASED16    0xFFFFu
#define FEE_ERASED32    0xFFFFFFFFu
#define FEE_WRITE_DONE  0x00000000u /* 완료 커밋 마커 값 */

#define FEE_BANK_HDR_SIZE 8u  /* sizeof(Fee_BankHeader) */
#define FEE_INST_HDR_SIZE 12u /* sizeof(Fee_InstHeader) */
#define FEE_MAX_DATA      16u /* config 의 가장 큰 블록 크기 */

/* 뱅크 맨 앞에 놓이는 헤더. magic 으로 유효성을, seqNo 로 최신 활성 뱅크를 가린다. */
typedef struct
{
    uint32_t magic;
    uint32_t seqNo;
} Fee_BankHeader;

/* 블록 인스턴스 헤더. 뒤에 length 바이트의 데이터가 이어진다.
 * writeState 는 데이터까지 다 쓴 뒤 "맨 마지막"에 기록하는 완료 마커다.
 * 쓰다가 전원이 나가면 writeState 가 erased(0xFFFFFFFF)로 남아 무효 판정된다. */
typedef struct
{
    uint16_t blockNumber;
    uint16_t length;
    uint32_t dataCrc;
    uint32_t writeState;
} Fee_InstHeader;

/* 비동기 write 를 몇 단계로 나눠 진행한다 (각 단계는 Fls 작업 하나). */
typedef enum
{
    W_IDLE = 0,
    W_FMT_ERASE,  /* (활성 뱅크 없을 때) 뱅크 전체 erase */
    W_FMT_HEADER, /* 뱅크 헤더(magic/seqNo) 기록 */
    W_INST,       /* 인스턴스 헤더+데이터 기록 (writeState 는 미완료) */
    W_MARKER,     /* writeState 완료 마커를 맨 마지막에 기록 */
    W_FINISH      /* RAM 인덱스 갱신, 완료 */
} Fee_WStep;

/* ================= 드라이버 상태 ================= */
static struct
{
    MemIf_StatusType    status;
    MemIf_JobResultType result;
    uint32_t            activeBank; /* FEE_BANK_A_BASE / B / 0(유효 뱅크 없음) */
    uint32_t            activeSeq;
    uint32_t            writePtr;              /* 활성 뱅크의 다음 append 위치 */
    uint32_t            index[FEE_MAX_BLOCKS]; /* 블록별 최신 유효 인스턴스 헤더 오프셋. 0 = 없음 */

    /* --- write job 진행 상태 --- */
    Fee_WStep wstep;
    uint8_t   flsIssued; /* 1 = Fls 작업을 발주하고 완료를 기다리는 중 */
    int16_t   pos;       /* 쓰는 블록의 config 위치 */
    uint32_t  instOff;   /* 이번 인스턴스가 놓일 오프셋 */
    uint32_t  targetBank;
    uint32_t  newSeq;
    uint8_t   bankHdr[FEE_BANK_HDR_SIZE];
    uint8_t   stage[FEE_INST_HDR_SIZE + FEE_MAX_DATA]; /* 인스턴스(헤더+데이터) 스테이징. RAM */
    uint16_t  stageLen;
    uint8_t   marker[4];
} g_fee = {0}; /* status 가 0 = MEMIF_UNINIT 로 시작한다 */

/* --- 리틀엔디안 바이트 패킹 헬퍼 (struct 정렬 문제를 피하려 직접 조립한다) --- */
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

/* config 배열에서 이 블록 번호의 위치를 찾는다. 없으면 -1. */
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

/* 활성 뱅크를 append 순서대로 훑으며, 각 블록의 최신 유효 인스턴스를 index 에 기록하고
 * 다음 쓰기 위치(writePtr)를 찾는다. 앞으로 진행하므로 나중에 만난 것이 최신이다. */
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
            if ((pos >= 0) &&
                (ih.writeState == FEE_WRITE_DONE) &&
                (ih.length == Fee_BlockConfig[pos].blockSize))
            {
                g_fee.index[pos] = off; /* 유효한 최신본으로 갱신 */
            }
        }

        {
            uint32_t instSize = FEE_INST_HDR_SIZE + ih.length;
            instSize = (instSize + 3u) & ~3u; /* 4바이트 정렬 */
            if (instSize <= (FEE_INST_HDR_SIZE - 1u))
            {
                break; /* 방어: length 가 비정상이면 중단 */
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

    (void)FlexSPI_ReadData(FEE_BANK_A_BASE, (uint8_t *)&ha, FEE_BANK_HDR_SIZE);
    (void)FlexSPI_ReadData(FEE_BANK_B_BASE, (uint8_t *)&hb, FEE_BANK_HDR_SIZE);

    aValid = (uint8_t)(ha.magic == FEE_BANK_MAGIC);
    bValid = (uint8_t)(hb.magic == FEE_BANK_MAGIC);

    /* magic 이 유효한 뱅크 중 seqNo 가 큰 쪽이 활성이다. */
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
        g_fee.activeBank = 0u; /* 유효 뱅크 없음 = 빈 flash */
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
        return E_NOT_OK; /* 아직 한 번도 쓰인 적 없는 블록이다 */
    }

    if (FlexSPI_ReadData(g_fee.index[pos] + FEE_INST_HDR_SIZE + Offset, Buf, Length) != FLS_IP_OK)
    {
        return E_NOT_OK;
    }
    return E_OK;
}

Std_ReturnType Fee_Write(uint16_t BlockNumber, const uint8_t *Buf)
{
    int16_t  pos;
    uint16_t sz;
    uint16_t i;
    uint32_t crc;
    uint32_t instSize;

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

    sz  = Fee_BlockConfig[pos].blockSize;
    crc = CRC32_Compute(Buf, (uint32_t)sz);

    /* 인스턴스(헤더+데이터)를 RAM 스테이징 버퍼에 조립한다.
     * writeState 자리는 미완료(0xFFFFFFFF)로 두고, 완료 마커는 맨 마지막 단계에서 쓴다. */
    put16(&g_fee.stage[0], BlockNumber);
    put16(&g_fee.stage[2], sz);
    put32(&g_fee.stage[4], crc);
    put32(&g_fee.stage[8], FEE_ERASED32);
    for (i = 0u; i < sz; i++)
    {
        g_fee.stage[FEE_INST_HDR_SIZE + i] = Buf[i];
    }
    g_fee.stageLen = (uint16_t)(FEE_INST_HDR_SIZE + sz);
    put32(&g_fee.marker[0], FEE_WRITE_DONE);

    instSize = ((uint32_t)FEE_INST_HDR_SIZE + sz + 3u) & ~3u;

    g_fee.pos = pos;

    if (g_fee.activeBank == 0u)
    {
        /* 유효 뱅크가 없다 → 첫 write 이므로 Bank A 를 포맷하고 시작한다. */
        g_fee.targetBank = FEE_BANK_A_BASE;
        g_fee.newSeq     = g_fee.activeSeq + 1u; /* 0 -> 1 */
        put32(&g_fee.bankHdr[0], FEE_BANK_MAGIC);
        put32(&g_fee.bankHdr[4], g_fee.newSeq);
        g_fee.wstep = W_FMT_ERASE;
    }
    else
    {
        /* 활성 뱅크에 공간이 남아 있는지 확인한다. */
        if ((g_fee.writePtr + instSize) > (g_fee.activeBank + FEE_BANK_SIZE))
        {
            /* 뱅크가 꽉 찼다. F-6c 에서 GC(뱅크 전환)로 처리한다. 지금은 거부한다. */
            return E_NOT_OK;
        }
        g_fee.instOff = g_fee.writePtr;
        g_fee.wstep   = W_INST;
    }

    g_fee.flsIssued = 0u;
    g_fee.status    = MEMIF_BUSY;
    g_fee.result    = MEMIF_JOB_PENDING;
    return E_OK;
}

/* 한 번 호출될 때마다 write 상태 머신을 한 걸음 전진시킨다.
 * 각 단계는 Fls 작업 하나를 발주하고, 그 Fls job 이 끝나면 다음 단계로 넘어간다.
 * (스케줄러가 Fee_MainFunction 과 Fls_MainFunction 을 함께 주기적으로 부른다.) */
void Fee_MainFunction(void)
{
    if (g_fee.status != MEMIF_BUSY)
    {
        return;
    }

    /* 앞 단계에서 발주한 Fls 작업이 끝났는지 확인하고, 끝났으면 다음 단계로 넘어간다. */
    if (g_fee.flsIssued != 0u)
    {
        MemIf_JobResultType fr = Fls_GetJobResult();
        if (fr == MEMIF_JOB_PENDING)
        {
            return; /* 아래 Fls 가 아직 작업 중이다 */
        }
        g_fee.flsIssued = 0u;
        if (fr != MEMIF_JOB_OK)
        {
            g_fee.wstep  = W_IDLE;
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
            g_fee.wstep = W_FINISH;
            break;
        default:
            break;
        }
    }

    /* 현재 단계의 작업을 발주한다. */
    switch (g_fee.wstep)
    {
    case W_FMT_ERASE:
        (void)Fls_Erase(g_fee.targetBank, FEE_BANK_SIZE);
        g_fee.flsIssued = 1u;
        break;

    case W_FMT_HEADER:
        (void)Fls_Write(g_fee.targetBank, g_fee.bankHdr, FEE_BANK_HDR_SIZE);
        g_fee.flsIssued = 1u;
        break;

    case W_INST:
        (void)Fls_Write(g_fee.instOff, g_fee.stage, g_fee.stageLen);
        g_fee.flsIssued = 1u;
        break;

    case W_MARKER:
        /* writeState 필드(인스턴스 오프셋 +8)를 완료값으로 덮어써 커밋한다. */
        (void)Fls_Write(g_fee.instOff + 8u, g_fee.marker, 4u);
        g_fee.flsIssued = 1u;
        break;

    case W_FINISH:
    {
        uint32_t instSize = ((uint32_t)FEE_INST_HDR_SIZE + Fee_BlockConfig[g_fee.pos].blockSize + 3u) & ~3u;
        g_fee.index[g_fee.pos] = g_fee.instOff; /* 이제부터 이 블록의 최신본은 이 인스턴스다 */
        g_fee.writePtr         = g_fee.instOff + instSize;
        g_fee.wstep            = W_IDLE;
        g_fee.status           = MEMIF_IDLE;
        g_fee.result           = MEMIF_JOB_OK;
        break;
    }

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
