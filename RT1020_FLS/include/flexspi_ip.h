#ifndef FLEXSPI_IP_H
#define FLEXSPI_IP_H

#include <stdint.h>

/* ---- FlexSPI 레지스터 (RM Ch.27, base 0x402A_8000) ---- */
#define FLEXSPI_BASE            0x402A8000u
#define FLEXSPI_INTR            (*(volatile uint32_t *)(FLEXSPI_BASE + 0x014u))
#define FLEXSPI_LUTKEY          (*(volatile uint32_t *)(FLEXSPI_BASE + 0x018u))
#define FLEXSPI_LUTCR           (*(volatile uint32_t *)(FLEXSPI_BASE + 0x01Cu))
#define FLEXSPI_IPCR0           (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0A0u))
#define FLEXSPI_IPCR1           (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0A4u))
#define FLEXSPI_IPCMD           (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0B0u))
#define FLEXSPI_IPRXFCR         (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0B8u))
#define FLEXSPI_STS0            (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0E0u))
#define FLEXSPI_IPRXFSTS        (*(volatile uint32_t *)(FLEXSPI_BASE + 0x0F0u))
#define FLEXSPI_RFDR            ((volatile uint32_t *)(FLEXSPI_BASE + 0x100u))
#define FLEXSPI_LUT             ((volatile uint32_t *)(FLEXSPI_BASE + 0x200u))

/* INTR — 전부 write-1-to-clear */
#define FLEXSPI_INTR_IPCMDDONE  (1u << 0)   /* IP 명령 완료 */
#define FLEXSPI_INTR_IPCMDGE    (1u << 1)   /* 버스 사용 허가를 못 받음 */
#define FLEXSPI_INTR_IPCMDERR   (1u << 3)   /* 시퀀스 실행 오류 */

/* STS0 — 둘 다 1 이어야 컨트롤러가 완전히 놀고 있는 것 */
#define FLEXSPI_STS0_SEQIDLE    (1u << 0)
#define FLEXSPI_STS0_ARBIDLE    (1u << 1)
#define FLEXSPI_STS0_IDLE_MASK  (FLEXSPI_STS0_SEQIDLE | FLEXSPI_STS0_ARBIDLE)
#define FLEXSPI_IPRXFSTS_FILL_MASK      0xFFu

/* IPCR1 필드 */
#define FLEXSPI_IPCR1_IDATSZ(n)   ((uint32_t)(n) & 0xFFFFu)          /* 주고받을 바이트 수 */
#define FLEXSPI_IPCR1_ISEQID(n)   (((uint32_t)(n) & 0x0Fu) << 16)    /* 실행할 악보 번호 */
#define FLEXSPI_IPCR1_ISEQNUM(n)  (((uint32_t)(n) & 0x07u) << 24)    /* 연달아 실행할 악보 수 - 1 */

#define FLEXSPI_IPCMD_TRG         (1u << 0)   /* 방아쇠 */
#define FLEXSPI_IPRXFCR_CLRIPRXF  (1u << 0)   /* RX FIFO 비우기 */
#define FLEXSPI_LUTKEY_VALUE      0x5AF05AF0u
#define FLEXSPI_LUTCR_LOCK        (1u << 0)
#define FLEXSPI_LUTCR_UNLOCK      (1u << 1)

/* ---- LUT 슬롯 배정 ---- */
/* 0/1/3/4/5/8/9/11 은 부팅 FCB 가 이미 점유하고 있다 (shared/src/flexspi_nor_config.c). */
#define FLS_LUT_SEQ_READ_STATUS     2u
#define FLS_LUT_SEQ_READ_DATA       6u
#define FLS_LUT_SEQ_READ_JEDEC_ID   7u
#define FLS_LUT_SEQ_WRITE_ENABLE    10u
#define FLS_LUT_SEQ_WRITE_DISABLE   12u

/* Status-1 레지스터 비트 (0x05 로 읽는다) */
#define FLS_STATUS_WIP   (1u << 0)   /* Write In Progress — 내부 작업 중이면 1 */
#define FLS_STATUS_WEL   (1u << 1)   /* Write Enable Latch — WREN 후 1, 쓰기 한 번이면 자동 해제 */

typedef enum
{
    FLS_IP_OK = 0,
    FLS_IP_E_TIMEOUT,
    FLS_IP_E_CMDERR,
    FLS_IP_E_PARAM
} Fls_IpStatus;

#define FLS_IP_READ_MAX             32u

void         FlexSPI_InstallLut(void);
Fls_IpStatus FlexSPI_ReadJedecId(uint8_t id[3]);
Fls_IpStatus FlexSPI_ReadStatus(uint8_t *status);
Fls_IpStatus FlexSPI_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
Fls_IpStatus FlexSPI_WriteEnable(void);
Fls_IpStatus FlexSPI_WriteDisable(void);

#endif /* FLEXSPI_IP_H */

