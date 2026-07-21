#include "flexspi_ip.h"
#include "flexspi_nor_config.h"   /* FLEXSPI_LUT_SEQ, CMD_SDR, CMD_READ_SDR, PAD_1 을 그대로 재사용 */

/* IP 명령은 µs 단위 */
#define FLS_IP_TIMEOUT   1000000u

#ifndef STOP
#define STOP   0x00   /* 시퀀스 끝 — flexspi_nor_config.h 엔 없어 여기서만 정의 */
#endif

FLS_RAMFUNC static Fls_IpStatus wait_ip_cmd_done(void)
{
    uint32_t guard = FLS_IP_TIMEOUT;

    do
    {
        uint32_t intr = FLEXSPI_INTR;

        if (intr & (FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE))
        {
            FLEXSPI_INTR = FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE | FLEXSPI_INTR_IPCMDDONE;
            return FLS_IP_E_CMDERR;
        }

        if (intr & FLEXSPI_INTR_IPCMDDONE)
        {
            FLEXSPI_INTR = FLEXSPI_INTR_IPCMDDONE;   /* W1C */
            return FLS_IP_OK;
        }
    } while (--guard);

    return FLS_IP_E_TIMEOUT;
}

FLS_RAMFUNC static Fls_IpStatus wait_rx_fill(void)
{
    uint32_t guard=FLS_IP_TIMEOUT;

    while((FLEXSPI_IPRXFSTS&FLEXSPI_IPRXFSTS_FILL_MASK)==0)
    {
        if(--guard==0u)
        {
            return FLS_IP_E_TIMEOUT;
        }
    }
    return FLS_IP_OK;
}

/* 부팅 LUT 의 빈 슬롯에 우리 코드를 추가한다. 기존 슬롯은 읽지도 쓰지도 않는다. */
void FlexSPI_InstallLut(void)
{
    /* 컨트롤러가 명령을 하나도 물고 있지 않을 때만 LUT 를 만진다. */
    while ((FLEXSPI_STS0 & FLEXSPI_STS0_IDLE_MASK) != FLEXSPI_STS0_IDLE_MASK)
    {
        /* wait */
    }

    FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
    FLEXSPI_LUTCR  = FLEXSPI_LUTCR_UNLOCK;

    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x9F, CMD_READ_SDR, PAD_1, 0x04);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 1u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 3u] = 0u;

    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_STATUS + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x05, CMD_READ_SDR, PAD_1, 0x01);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_STATUS + 1u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_STATUS + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_STATUS + 3u] = 0u;

    

    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_DATA + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x0B, CMD_RADDR_SDR, PAD_1, 0x18);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_DATA + 1u] =
        FLEXSPI_LUT_SEQ(CMD_DUMMY_SDR, PAD_1, 0x08, CMD_READ_SDR, PAD_1, 0x04);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_DATA + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_DATA + 3u] = 0u;

    /* WRITE ENABLE */
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_ENABLE + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x06, STOP, PAD_1, 0x00);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_ENABLE + 1u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_ENABLE + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_ENABLE + 3u] = 0u;

    /* WRITE DISABLE */
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_DISABLE + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x04, STOP, PAD_1, 0x00);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_DISABLE + 1u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_DISABLE + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_WRITE_DISABLE + 3u] = 0u;

    FLEXSPI_LUT[4u * FLS_LUT_SEQ_SECTOR_ERASE + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x20, CMD_RADDR_SDR, PAD_1, 0x18);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_SECTOR_ERASE + 1u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_SECTOR_ERASE + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_SECTOR_ERASE + 3u] = 0u;

    FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
    FLEXSPI_LUTCR  = FLEXSPI_LUTCR_LOCK;
}

/* ===== 위험 구간 — 여기부터는 ITCM 에서 실행된다 =====
* flash 를 읽는 행위(문자열 출력, const 접근, flash 함수 호출)를 절대 하지 말 것.
* 오직 FlexSPI 레지스터와 스택(DTCM)만 만진다. */
FLS_RAMFUNC static Fls_IpStatus erase_core(uint32_t addr, Fls_EraseTrace *trace)
{
    Fls_IpStatus st;
    uint8_t      sr;
    uint32_t     n = 0u;

    /* 쓰기 가능 여부 확인 (WEL: 0 -> 1) */
    st = FlexSPI_WriteEnable();
    if (st != FLS_IP_OK)
    {
        return st;
    }

    /* sector erase. 이 순간부터 flash 는 읽기 없음. */
    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    FLEXSPI_INTR     = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;
    FLEXSPI_IPCR0    = addr;
    FLEXSPI_IPCR1    = FLEXSPI_IPCR1_ISEQID(FLS_LUT_SEQ_SECTOR_ERASE)
                    | FLEXSPI_IPCR1_ISEQNUM(0u)
                    | FLEXSPI_IPCR1_IDATSZ(0u);
    FLEXSPI_IPCMD    = FLEXSPI_IPCMD_TRG;

    st = wait_ip_cmd_done();
    if (st != FLS_IP_OK)
    {
        return st;
    }

    st = FlexSPI_ReadStatus(&sr);
    if (st != FLS_IP_OK)
    {
        return st;
    }
    trace->sr_after_cmd = sr;

    /* WIP 폴링. 수십 ms 동안 flash 는 잠들어 있고, 이 루프만이 RAM 에서 돈다. */
    while ((sr & FLS_STATUS_WIP) != 0u)
    {
        st = FlexSPI_ReadStatus(&sr);
        if (st != FLS_IP_OK)
        {
            return st;
        }
        n++;
    }
    trace->poll_count = n;

    return FLS_IP_OK;
}
/* ===== 위험 구간 끝 ===== */

/* 가드를 통과시킨 뒤 위험 구간으로 넘긴다. 이 함수 자체는 flash 에 있어도 된다 —
* 판정에 쓰는 상수를 전부 읽고 난 뒤에야 erase 가 시작되기 때문이다. */
Fls_IpStatus Fls_EraseSector(uint32_t addr, Fls_EraseTrace *trace)
{
    Fls_IpStatus st;

    if (trace == 0)
    {
        return FLS_IP_E_PARAM;
    }

    /* 허용 영역 안이고 섹터 경계에 정렬돼 있어야 한다.
    * 이미지 영역으로 향하는 erase 는 여기서 끝난다. */
    if ((addr < FLS_WRITE_AREA_BASE) ||
        (addr >= FLS_WRITE_AREA_LIMIT) ||
        ((addr % FLS_SECTOR_SIZE) != 0u))
    {
        return FLS_IP_E_FORBIDDEN;
    }

    trace->sr_after_cmd = 0u;
    trace->poll_count   = 0u;

    /* 인터럽트를 막는다. ISR 은 flash 에 있어서, erase 중에 뛰면 그대로 죽는다.
    * (지금 이 프로젝트는 인터럽트를 안 쓰지만 규칙은 규칙이다.) */
    __asm volatile ("cpsid i" ::: "memory");
    st = erase_core(addr, trace);
    __asm volatile ("cpsie i" ::: "memory");

    return st;
}

Fls_IpStatus FlexSPI_ReadJedecId(uint8_t id[3])
{
    Fls_IpStatus st;
    uint32_t     word;

    /* 이전 명령이 남긴 데이터와 플래그를 먼저 치운다. */
    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    FLEXSPI_INTR     = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;

    FLEXSPI_IPCR0 = 0u;

    FLEXSPI_IPCR1 = FLEXSPI_IPCR1_ISEQID(FLS_LUT_SEQ_READ_JEDEC_ID)
                | FLEXSPI_IPCR1_ISEQNUM(0u)
                | FLEXSPI_IPCR1_IDATSZ(3u);

    FLEXSPI_IPCMD = FLEXSPI_IPCMD_TRG;

    /* 완료 대기 */
    st = wait_ip_cmd_done();
    if (st != FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    st = wait_rx_fill();
    if (st != FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR|=FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    word  = FLEXSPI_RFDR[0];
    id[0] = (uint8_t)(word & 0xFFu);           /* manufacturer */
    id[1] = (uint8_t)((word >> 8) & 0xFFu);    /* memory type */
    id[2] = (uint8_t)((word >> 16) & 0xFFu);   /* capacity (2^n 바이트) */

    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    return FLS_IP_OK;
}

FLS_RAMFUNC Fls_IpStatus FlexSPI_ReadStatus(uint8_t *status)
{
    Fls_IpStatus st;

    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    FLEXSPI_INTR = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;

    FLEXSPI_IPCR0 = 0u; /* status 읽기는 주소가 없다 */
    FLEXSPI_IPCR1 = FLEXSPI_IPCR1_ISEQID(FLS_LUT_SEQ_READ_STATUS) | FLEXSPI_IPCR1_ISEQNUM(0u) | FLEXSPI_IPCR1_IDATSZ(1u);
    FLEXSPI_IPCMD = FLEXSPI_IPCMD_TRG;

    st = wait_ip_cmd_done();
    if (st != FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    st=wait_rx_fill();
    if(st!=FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR|=FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    *status = (uint8_t)(FLEXSPI_RFDR[0] & 0xFFu);
    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    return FLS_IP_OK;
}

Fls_IpStatus FlexSPI_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    Fls_IpStatus st;
    uint32_t words;
    uint32_t i;

    if (len == 0u || len > FLS_IP_READ_MAX)
    {
        return FLS_IP_E_PARAM;
    }

    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    FLEXSPI_INTR = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;

    FLEXSPI_IPCR0 = addr;
    FLEXSPI_IPCR1 = FLEXSPI_IPCR1_ISEQID(FLS_LUT_SEQ_READ_DATA) | FLEXSPI_IPCR1_ISEQNUM(0u) | FLEXSPI_IPCR1_IDATSZ(len);
    FLEXSPI_IPCMD = FLEXSPI_IPCMD_TRG;

    st = wait_ip_cmd_done();
    if (st != FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    st=wait_rx_fill();
    if(st!=FLS_IP_OK)
    {
        FLEXSPI_IPRXFCR|=FLEXSPI_IPRXFCR_CLRIPRXF;
        return st;
    }

    words = (len + 3u) / 4u;
    for (i = 0u; i < words; ++i)
    {
        uint32_t w = FLEXSPI_RFDR[i];
        uint32_t b;
        for (b = 0u; b < 4u && (i * 4u + b) < len; ++b)
        {
            buf[i * 4u + b] = (uint8_t)((w >> (8u * b)) & 0xFFu);
        }
    }
    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    return FLS_IP_OK;
}

FLS_RAMFUNC static Fls_IpStatus run_cmd_no_data(uint32_t seq_id)
{
    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    FLEXSPI_INTR     = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;

    FLEXSPI_IPCR0 = 0u;
    FLEXSPI_IPCR1 = FLEXSPI_IPCR1_ISEQID(seq_id)
                | FLEXSPI_IPCR1_ISEQNUM(0u)
                | FLEXSPI_IPCR1_IDATSZ(0u);
    FLEXSPI_IPCMD = FLEXSPI_IPCMD_TRG;

    return wait_ip_cmd_done();
}

FLS_RAMFUNC Fls_IpStatus FlexSPI_WriteEnable(void)
{
    return run_cmd_no_data(FLS_LUT_SEQ_WRITE_ENABLE);
}

FLS_RAMFUNC Fls_IpStatus FlexSPI_WriteDisable(void)
{
    return run_cmd_no_data(FLS_LUT_SEQ_WRITE_DISABLE);
}
