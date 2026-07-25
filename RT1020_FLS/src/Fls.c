#include "Fls.h"
#include "Det.h"
#include "flexspi_ip.h" /* Fls_EraseSector / Fls_ProgramPage / FlexSPI_ReadData 를 재사용한다 */

/* 진행 중인 job 의 종류 */
typedef enum
{
    JOB_NONE = 0,
    JOB_ERASE,
    JOB_WRITE,
    JOB_READ
} Fls_JobType;

/* 드라이버의 내부 상태. 요청 함수가 여기에 job 을 적어두면
 * Fls_MainFunction 이 한 조각씩 진행한다. */
static struct
{
    const Fls_ConfigType *cfg;
    MemIf_StatusType      status;
    MemIf_JobResultType   result;
    Fls_JobType           job;
    Fls_AddressType       addr;      /* 다음에 처리할 flash 오프셋 */
    Fls_LengthType        remaining; /* 아직 처리하지 못한 바이트 수 */
    const uint8_t        *src;       /* write 원본. RAM 에 있어야 한다 */
    uint8_t              *dst;       /* read 목적지 */
} g_fls = {0}; /* status 가 0 = MEMIF_UNINIT 로 시작한다 */

/* 요청한 [주소, 길이] 가 config 의 허용 영역 안에 완전히 들어오는지 검사한다. */
static uint8_t in_range(Fls_AddressType a, Fls_LengthType len)
{
    Fls_AddressType base = g_fls.cfg->baseAddress;
    Fls_AddressType end  = base + g_fls.cfg->totalSize;

    return (uint8_t)((a >= base) && (a <= end) && (len <= (end - a)));
}

void Fls_Init(const Fls_ConfigType *ConfigPtr)
{
    if (ConfigPtr == 0)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_INIT, FLS_E_PARAM_DATA);
        return;
    }

    FlexSPI_InstallLut(); /* LUT 악보를 한 번 설치한다 (F-2 ~ F-4 에서 만든 것) */

    g_fls.cfg    = ConfigPtr;
    g_fls.status = MEMIF_IDLE;
    g_fls.result = MEMIF_JOB_OK;
    g_fls.job    = JOB_NONE;
}

Std_ReturnType Fls_Erase(Fls_AddressType TargetAddress, Fls_LengthType Length)
{
    if (g_fls.status == MEMIF_UNINIT)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_ERASE, FLS_E_UNINIT);
        return E_NOT_OK;
    }
    if (g_fls.status == MEMIF_BUSY)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_ERASE, FLS_E_BUSY);
        return E_NOT_OK;
    }
    /* erase 는 섹터 단위로만 한다. 주소와 길이가 모두 섹터에 정렬되어야 한다. */
    if ((Length == 0u) ||
        ((TargetAddress % g_fls.cfg->sectorSize) != 0u) ||
        ((Length % g_fls.cfg->sectorSize) != 0u))
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_ERASE, FLS_E_PARAM_LENGTH);
        return E_NOT_OK;
    }
    if (!in_range(TargetAddress, Length))
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_ERASE, FLS_E_PARAM_ADDRESS);
        return E_NOT_OK;
    }

    g_fls.job       = JOB_ERASE;
    g_fls.addr      = TargetAddress;
    g_fls.remaining = Length;
    g_fls.status    = MEMIF_BUSY;
    g_fls.result    = MEMIF_JOB_PENDING;
    return E_OK; /* 요청만 접수하고 곧바로 반환한다. 실제 작업은 Fls_MainFunction 이 한다 */
}

Std_ReturnType Fls_Write(Fls_AddressType TargetAddress, const uint8_t *SourceAddressPtr, Fls_LengthType Length)
{
    if (g_fls.status == MEMIF_UNINIT)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_WRITE, FLS_E_UNINIT);
        return E_NOT_OK;
    }
    if (g_fls.status == MEMIF_BUSY)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_WRITE, FLS_E_BUSY);
        return E_NOT_OK;
    }
    if (SourceAddressPtr == 0)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_WRITE, FLS_E_PARAM_DATA);
        return E_NOT_OK;
    }
    if (Length == 0u)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_WRITE, FLS_E_PARAM_LENGTH);
        return E_NOT_OK;
    }
    if (!in_range(TargetAddress, Length))
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_WRITE, FLS_E_PARAM_ADDRESS);
        return E_NOT_OK;
    }

    g_fls.job       = JOB_WRITE;
    g_fls.addr      = TargetAddress;
    g_fls.src       = SourceAddressPtr;
    g_fls.remaining = Length;
    g_fls.status    = MEMIF_BUSY;
    g_fls.result    = MEMIF_JOB_PENDING;
    return E_OK;
}

Std_ReturnType Fls_Read(Fls_AddressType SourceAddress, uint8_t *TargetAddressPtr, Fls_LengthType Length)
{
    if (g_fls.status == MEMIF_UNINIT)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_READ, FLS_E_UNINIT);
        return E_NOT_OK;
    }
    if (g_fls.status == MEMIF_BUSY)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_READ, FLS_E_BUSY);
        return E_NOT_OK;
    }
    if (TargetAddressPtr == 0)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_READ, FLS_E_PARAM_DATA);
        return E_NOT_OK;
    }
    if (Length == 0u)
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_READ, FLS_E_PARAM_LENGTH);
        return E_NOT_OK;
    }
    if (!in_range(SourceAddress, Length))
    {
        Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_READ, FLS_E_PARAM_ADDRESS);
        return E_NOT_OK;
    }

    g_fls.job       = JOB_READ;
    g_fls.addr      = SourceAddress;
    g_fls.dst       = TargetAddressPtr;
    g_fls.remaining = Length;
    g_fls.status    = MEMIF_BUSY;
    g_fls.result    = MEMIF_JOB_PENDING;
    return E_OK;
}

/* 한 번 호출될 때마다 연산 하나(섹터 1개 / 페이지 조각 1개 / 읽기 조각 1개)를
 * 끝까지 완료한다. 그 연산 동안에만 flash 가 busy 이고, 이 함수가 반환하는
 * 시점에는 언제나 flash 가 idle 이므로 호출자가 안전하게 flash 코드를 실행할 수 있다. */
void Fls_MainFunction(void)
{
    Fls_EraseTrace trace;
    Fls_IpStatus   hw;
    Fls_LengthType chunk;

    if (g_fls.status != MEMIF_BUSY)
    {
        return; /* 진행할 job 이 없다 */
    }

    switch (g_fls.job)
    {
    case JOB_ERASE:
        hw = Fls_EraseSector(g_fls.addr, &trace); /* 섹터 하나를 지운다 (블로킹, 수십 ms) */
        if (hw != FLS_IP_OK)
        {
            goto job_failed;
        }
        g_fls.addr      += g_fls.cfg->sectorSize;
        g_fls.remaining -= g_fls.cfg->sectorSize;
        break;

    case JOB_WRITE:
        chunk = (g_fls.remaining < g_fls.cfg->maxWriteChunk)
                    ? g_fls.remaining
                    : g_fls.cfg->maxWriteChunk;
        {
            /* 한 번의 program 이 페이지 경계를 넘지 않도록 자른다. */
            Fls_LengthType toPageEnd = g_fls.cfg->pageSize - (g_fls.addr % g_fls.cfg->pageSize);
            if (chunk > toPageEnd)
            {
                chunk = toPageEnd;
            }
        }
        hw = Fls_ProgramPage(g_fls.addr, g_fls.src, chunk, &trace);
        if (hw != FLS_IP_OK)
        {
            goto job_failed;
        }
        g_fls.addr      += chunk;
        g_fls.src       += chunk;
        g_fls.remaining -= chunk;
        break;

    case JOB_READ:
        chunk = (g_fls.remaining < FLS_IP_READ_MAX) ? g_fls.remaining : FLS_IP_READ_MAX;
        hw    = FlexSPI_ReadData(g_fls.addr, g_fls.dst, chunk);
        if (hw != FLS_IP_OK)
        {
            goto job_failed;
        }
        g_fls.addr      += chunk;
        g_fls.dst       += chunk;
        g_fls.remaining -= chunk;
        break;

    default:
        g_fls.status = MEMIF_IDLE;
        return;
    }

    if (g_fls.remaining == 0u)
    {
        /* 남은 조각이 없다. job 이 성공적으로 끝났다. */
        g_fls.job    = JOB_NONE;
        g_fls.status = MEMIF_IDLE;
        g_fls.result = MEMIF_JOB_OK;
    }
    return;

job_failed:
    g_fls.job    = JOB_NONE;
    g_fls.status = MEMIF_IDLE;
    g_fls.result = MEMIF_JOB_FAILED;
    Det_ReportError(FLS_MODULE_ID, FLS_INSTANCE_ID, FLS_SID_MAINFUNCTION, FLS_E_PARAM_ADDRESS);
}

MemIf_StatusType Fls_GetStatus(void)
{
    return g_fls.status;
}

MemIf_JobResultType Fls_GetJobResult(void)
{
    return g_fls.result;
}
