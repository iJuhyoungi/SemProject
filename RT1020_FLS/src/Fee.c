#include "Fee.h"
#include "Det.h"
#include "flexspi_ip.h" /* FlexSPI_ReadData 를 재사용한다 (읽기는 flash 가 busy 가 아니라 동기로 처리) */

/* ================= on-flash 레이아웃 =================
 * Fee 영역은 Fls 허용 영역(0x700000~0x800000) 안의 두 뱅크로 구성된다.
 * F-4/F-5 실험 섹터(0x7FF000)와 겹치지 않도록 앞쪽에 배치했다. */
#define FEE_BANK_A_BASE 0x00780000u
#define FEE_BANK_B_BASE 0x00788000u
#define FEE_BANK_SIZE   0x00008000u /* 32KB = 8 sector */

#define FEE_BANK_MAGIC  0x31454546u /* 바이트로 'F','E','E','1' (LE) */
#define FEE_ERASED16    0xFFFFu
#define FEE_WRITE_DONE  0x00000000u /* 완료 커밋 마커 값 */

#define FEE_BANK_HDR_SIZE 8u  /* sizeof(Fee_BankHeader) */
#define FEE_INST_HDR_SIZE 12u /* sizeof(Fee_InstHeader) */

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

/* ================= 드라이버 상태 ================= */
static struct
{
    MemIf_StatusType    status;
    MemIf_JobResultType result;
    uint32_t            activeBank; /* FEE_BANK_A_BASE / B / 0(유효 뱅크 없음) */
    uint32_t            activeSeq;
    uint32_t            writePtr;              /* 활성 뱅크의 다음 append 위치 */
    uint32_t            index[FEE_MAX_BLOCKS]; /* 블록별 최신 유효 인스턴스 헤더 오프셋. 0 = 없음 */
} g_fee = {0}; /* status 가 0 = MEMIF_UNINIT 로 시작한다 */

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
            if (instSize <= FEE_INST_HDR_SIZE - 1u)
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

/* F-6a 에서는 스캔·읽기가 모두 동기라 진행할 비동기 job 이 없다.
 * F-6b 에서 쓰기(append)를 넣으면 여기서 Fls job 을 구동하게 된다. */
void Fee_MainFunction(void)
{
}

MemIf_StatusType Fee_GetStatus(void)
{
    return g_fee.status;
}

MemIf_JobResultType Fee_GetJobResult(void)
{
    return g_fee.result;
}
