#include "flexspi_ip.h"
#include "flexspi_nor_config.h"   /* FLEXSPI_LUT_SEQ, CMD_SDR, CMD_READ_SDR, PAD_1 을 그대로 재사용 */

/* IP 명령은 µs 단위 */
#define FLS_IP_TIMEOUT   1000000u

static Fls_IpStatus wait_ip_cmd_done(void)
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
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 1u] = 0u;   /* STOP */
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 2u] = 0u;
    FLEXSPI_LUT[4u * FLS_LUT_SEQ_READ_JEDEC_ID + 3u] = 0u;

    FLEXSPI_LUTKEY = FLEXSPI_LUTKEY_VALUE;
    FLEXSPI_LUTCR  = FLEXSPI_LUTCR_LOCK;
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

    word  = FLEXSPI_RFDR[0];
    id[0] = (uint8_t)(word & 0xFFu);           /* manufacturer */
    id[1] = (uint8_t)((word >> 8) & 0xFFu);    /* memory type */
    id[2] = (uint8_t)((word >> 16) & 0xFFu);   /* capacity (2^n 바이트) */

    FLEXSPI_IPRXFCR |= FLEXSPI_IPRXFCR_CLRIPRXF;
    return FLS_IP_OK;
}
