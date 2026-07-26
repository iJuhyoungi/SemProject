#include "flexspi_ip.h"
#include "flexspi_nor_config.h"   /* FLEXSPI_LUT_SEQ, CMD_SDR, CMD_READ_SDR, PAD_1 을 그대로 재사용 */

/* IP 명령은 µs 단위 */
#define FLS_IP_TIMEOUT   1000000u

#ifndef STOP
#define STOP   0x00   /* 시퀀스 끝 — flexspi_nor_config.h 엔 없어 여기서만 정의 */
#endif

#ifndef CMD_WRITE_SDR
#define CMD_WRITE_SDR  0x08   /* 데이터 송신 opcode — 헤더엔 READ(0x09)만 있어 여기서 정의 */
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

    /* PAGE PROGRAM: 0x02 → 24비트 주소 → 데이터 송신, 전부 1가닥 */
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_PAGE_PROGRAM + 0u] =
        FLEXSPI_LUT_SEQ(CMD_SDR, PAD_1, 0x02, CMD_RADDR_SDR, PAD_1, 0x18);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_PAGE_PROGRAM + 1u] =
        FLEXSPI_LUT_SEQ(CMD_WRITE_SDR, PAD_1, 0x04, STOP, PAD_1, 0x00);
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_PAGE_PROGRAM + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_PAGE_PROGRAM + 3u] = 0u;

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

/* Cortex-M7 I-cache 제어 (SCB_CCR bit17=IC). SCB 는 flash 가 아니라 시스템 영역이라 안전.
 * flash 를 쓰는 동안 I-cache 가 flash 로 speculative linefill 을 하면, 그 순간 flash 는
 * busy 라 버스 에러(IBUSERR)로 죽는다. 그래서 작업 구간 동안 I-cache 를 꺼둔다. */
#define SCB_CCR_REG   (*(volatile uint32_t *)0xE000ED14u)
#define SCB_CCR_IC    (1u << 17)
#define SCB_ICIALLU   (*(volatile uint32_t *)0xE000EF50u)

static void icache_disable(void)
{
    __asm volatile ("dsb 0xf" ::: "memory");
    SCB_CCR_REG &= ~SCB_CCR_IC;
    __asm volatile ("dsb 0xf" ::: "memory");
    __asm volatile ("isb 0xf" ::: "memory");
}

static void icache_enable(void)
{
    __asm volatile ("dsb 0xf" ::: "memory");
    SCB_ICIALLU = 0u;                  /* 전체 무효화 */
    __asm volatile ("dsb 0xf" ::: "memory");
    SCB_CCR_REG |= SCB_CCR_IC;
    __asm volatile ("dsb 0xf" ::: "memory");
    __asm volatile ("isb 0xf" ::: "memory");
}

/* ===== 위험 구간 — ITCM 에서 실행 =====
* 보낼 데이터(data)도 반드시 RAM(스택/DTCM)에 있어야 한다.
* flash 의 문자열/const 를 넘기면 여기서 그것을 읽다 죽는다. */
FLS_RAMFUNC static Fls_IpStatus program_core(uint32_t addr, const uint8_t *data,
                                            uint32_t len, Fls_EraseTrace *trace)
{
    Fls_IpStatus st;
    uint8_t      sr;
    uint32_t     n = 0u;
    uint32_t     i;
    uint32_t     b;

    /* ① 쓰기 허락 (WEL: 0 -> 1) */
    st = FlexSPI_WriteEnable();
    if (st != FLS_IP_OK)
    {
        return st;
    }

    /* ② TX FIFO 비우고(부팅 잔재 0x0200 제거) → 명령 설정 → 트리거 → 곧바로 TFDR 채우기.
     * 작은 데이터(<=FIFO)는 명령 컨텍스트가 활성인 트리거 직후에 TFDR 을 직접 쓰면
     * data 위상 전에 FIFO 에 실린다. IPTXWE(edge-latched) 게이팅은 오히려 채우기를 건너뛴다. */
    /* NXP SDK FLEXSPI_WriteBlocking 순서: FIFO 클리어 → 명령 설정 → 트리거 →
     * IPTXWE(빈 공간 생김) 를 폴링하며 워터마크씩 채우고 push. IPTXWE 는 사전 클리어하지 않는다. */
    FLEXSPI_IPTXFCR |= FLEXSPI_IPTXFCR_CLRIPTXF;
    FLEXSPI_INTR  = FLEXSPI_INTR_IPCMDDONE | FLEXSPI_INTR_IPCMDERR | FLEXSPI_INTR_IPCMDGE;
    FLEXSPI_IPCR0 = addr;
    FLEXSPI_IPCR1 = FLEXSPI_IPCR1_ISEQID(FLS_LUT_SEQ_PAGE_PROGRAM)
                | FLEXSPI_IPCR1_ISEQNUM(0u)
                | FLEXSPI_IPCR1_IDATSZ(len);
    FLEXSPI_IPCMD = FLEXSPI_IPCMD_TRG;

    /* 데이터를 TX FIFO 에 채운다. 워터마크(2워드=8B) 단위로 TFDR[0]/TFDR[1] 에 쓰고 push 한다.
     * 이 하드웨어는 작은 write 에서 IPTXWE 가 첫 워터마크 뒤로는 재신호되지 않아, 실질적으로
     * 한 워터마크(8B)까지만 안정적으로 전송된다. 그래서 더 큰 write 는 Fls_MainFunction 이
     * maxWriteChunk(=8) 단위로 쪼개어 여러 번의 페이지 프로그램으로 넘긴다. */
    {
        uint32_t words = (len + 3u) / 4u;
        for (i = 0u; i < words; i++)
        {
            uint32_t w = 0u;
            for (b = 0u; (b < 4u) && (((i * 4u) + b) < len); b++)
            {
                w |= (uint32_t)data[(i * 4u) + b] << (8u * b);
            }
            FLEXSPI_TFDR[i & 1u] = w;
            if (((i & 1u) == 1u) || (i == (words - 1u)))
            {
                FLEXSPI_INTR = FLEXSPI_INTR_IPTXWE; /* 워터마크 또는 마지막 워드마다 push */
            }
        }
    }

    st = wait_ip_cmd_done();
    if (st != FLS_IP_OK)
    {
        FLEXSPI_IPTXFCR |= FLEXSPI_IPTXFCR_CLRIPTXF;
        return st;
    }

    /* ④ 첫 status — program 이 진짜 시작됐는지 (WIP=1) */
    st = FlexSPI_ReadStatus(&sr);
    if (st != FLS_IP_OK)
    {
        return st;
    }
    trace->sr_after_cmd = sr;

    /* ⑤ WIP 폴링. program 은 1ms 미만이라 erase 보다 훨씬 짧다.
     * WIP 이 끝내 안 내려가면 무한 루프에 빠지므로 상한을 둔다 (진단용). */
    while ((sr & FLS_STATUS_WIP) != 0u)
    {
        st = FlexSPI_ReadStatus(&sr);
        if (st != FLS_IP_OK)
        {
            trace->poll_count = n;
            return st;
        }
        n++;
        if (n >= 2000000u)
        {
            trace->poll_count = n;
            FLEXSPI_IPTXFCR |= FLEXSPI_IPTXFCR_CLRIPTXF;
            return FLS_IP_E_TIMEOUT;
        }
    }
    trace->poll_count = n;

    FLEXSPI_IPTXFCR |= FLEXSPI_IPTXFCR_CLRIPTXF;
    return FLS_IP_OK;
}
/* ===== 위험 구간 끝 ===== */

/* 가드 통과 후 위험 구간으로 넘긴다. erase 와 같은 구조. */
Fls_IpStatus Fls_ProgramPage(uint32_t addr, const uint8_t *data, uint32_t len, Fls_EraseTrace *trace)
{
    Fls_IpStatus st;

    if ((data == 0) || (trace == 0))
    {
        return FLS_IP_E_PARAM;
    }
    /* F-4c 는 한 번의 FIFO 채움으로 끝나는 크기만 다룬다 (전체 256B 페이지는 F-5 몫). */
    if ((len == 0u) || (len > FLS_IP_READ_MAX))
    {
        return FLS_IP_E_PARAM;
    }
    /* 허용 영역 밖이면 이미지 보호를 위해 거부 */
    if ((addr < FLS_WRITE_AREA_BASE) || (addr >= FLS_WRITE_AREA_LIMIT))
    {
        return FLS_IP_E_FORBIDDEN;
    }
    /* page program 은 256B 페이지 경계를 넘을 수 없다 (넘으면 페이지 처음으로 감긴다) */
    if (((addr & (FLS_PAGE_SIZE - 1u)) + len) > FLS_PAGE_SIZE)
    {
        return FLS_IP_E_PARAM;
    }

    trace->sr_after_cmd = 0u;
    trace->poll_count   = 0u;

    __asm volatile ("cpsid i" ::: "memory");
    icache_disable();                       /* 작업 구간 동안 flash 로의 speculative fetch 차단 */
    st = program_core(addr, data, len, trace);
    icache_enable();
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
