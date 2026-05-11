#include <stdint.h>
#include "recovery_protocol.h"
#include "uart.h"
#include "crc32.h"
#include "rt1020_regs.h"

/* === Helper Functions === */
/*
 * Little-endian 4-byte → uint32_t
 * UART RX가 byte 단위라서, Little-endian으로 4바이트를 조립하는 helper 함수 필요. (예: size, CRC 수신 시)
 */

#define STAGE2_FLASH_BASE 0x60004000u

/* flash chip 단위 (bootctrl_rom_api.c와 일치) */
#define FLASH_SECTOR_SIZE 4096u
#define FLASH_PAGE_SIZE 256

extern int BootCtrl_LowLevel_EraseSector(uint32_t address);
extern int BootCtrl_LowLevel_ProgramPage(uint32_t address, const void *src, uint32_t size);

static uint32_t read_uint32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

recovery_state_t Recovery_HandleIdle(recovery_ctx_t *ctx)
{
    (void)ctx;
    UART1_SendString("[RECOVERY] waiting for handshake...\r\n");

    /* Phase 1: HOST → BOARD: "RECV?\n" (6 bytes ASCII) */
    uint8_t buf[6];
    UART1_RxBytes(buf, 6);

    if (buf[0] == 'R' && buf[1] == 'E' && buf[2] == 'C' &&
        buf[3] == 'V' && buf[4] == '?' && buf[5] == '\n')
    {
        return STATE_HANDSHAKE;
    }

    UART1_SendString("[RECOVERY] unexpected handshake, retry\r\n");
    return STATE_IDLE; /* match 실패 시 IDLE 재진입 (host 재시도 가능) */
}

recovery_state_t Recovery_HandleHandshake(recovery_ctx_t *ctx)
{
    (void)ctx;
    /* Phase 1 응답: BOARD → HOST: "READY!\r\n" */
    UART1_SendString(MSG_READY);
    return STATE_RECV_SIZE;
}

recovery_state_t Recovery_HandleRecvSize(recovery_ctx_t *ctx)
{
    /* Phase 2: HOST → BOARD: size (4 bytes LE) */
    uint8_t size_bytes[4];
    UART1_RxBytes(size_bytes, 4);

    uint32_t size = read_uint32_le(size_bytes);

    if (size == 0u || size > MAX_STAGE2_SIZE)
    {
        UART1_SendString(MSG_TOOBIG);
        return STATE_IDLE; /* host 재시도 가능 */
    }

    ctx->recv_size = size;
    UART1_SendString(MSG_ACK);
    return STATE_RECV_DATA;
}

recovery_state_t Recovery_HandleRecvData(recovery_ctx_t *ctx)
{
    /* Phase 3: HOST → BOARD: data (size bytes, raw binary) */
    UART1_RxBytes(ctx->buffer, ctx->recv_size);
    return STATE_RECV_CRC;
}

recovery_state_t Recovery_HandleRecvCRC(recovery_ctx_t *ctx)
{
    /* Phase 4: HOST → BOARD: crc32 (4 bytes LE) */
    uint8_t crc_bytes[4];
    UART1_RxBytes(crc_bytes, 4);

    ctx->recv_crc = read_uint32_le(crc_bytes);
    return STATE_VERIFY_CRC;
}

recovery_state_t Recovery_HandleVerifyCRC(recovery_ctx_t *ctx)
{
    /* Phase 4 검증: CRC32_Compute(buffer, size) vs received_crc */
    uint32_t computed = CRC32_Compute(ctx->buffer, ctx->recv_size);

    if (computed != ctx->recv_crc)
    {
        UART1_SendString(MSG_CRCFAIL);
        return STATE_IDLE; /* host 재시도 (transmission 오류) */
    }

    UART1_SendString(MSG_CRCOK);
    return STATE_FLASH;
}

recovery_state_t Recovery_HandleFlash(recovery_ctx_t *ctx)
{
    (void)ctx;
    /* 1. Erase Stage 2 영역 */
    UART1_SendString("[RECOVERY] erasing stage2...\r\n");

    uint32_t sectors = (ctx->recv_size + FLASH_SECTOR_SIZE - 1u) / FLASH_SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; ++i)
    {
        uint32_t addr = STAGE2_FLASH_BASE + (i * FLASH_SECTOR_SIZE);
        if (!BootCtrl_LowLevel_EraseSector(addr))
        {
            UART1_SendString("[RECOVERY] erasing stage2 failed\r\n");
            return STATE_FAIL;
        }
    }

    /* 2. Program Stage 2 (256B page 단위) 영역 */
    UART1_SendString("[RECOVERY] programming stage2...\r\n");

    uint32_t pages = (ctx->recv_size + FLASH_PAGE_SIZE - 1u) / FLASH_PAGE_SIZE;

    /*
     * recv_size 가 256 의 배수가 아닐 수 있음.
     * ProgramPage 는 size != 256 면 fail 반환하므로, 마지막 page 도 256B 로 호출해야 함.
     * → buffer 의 [recv_size .. pages*256] 영역을 0xFF (erased state) 로 채워서
     *   flash 에는 실질적으로 "아무것도 안 쓴 것" 과 동일하게 만든다.
     */

    uint32_t padded_total = pages * FLASH_PAGE_SIZE;
    for (uint32_t i = ctx->recv_size; i < padded_total; ++i)
    {
        ctx->buffer[i] = 0xFFu;
    }

    for (uint32_t i = 0; i < pages; ++i)
    {
        uint32_t addr = STAGE2_FLASH_BASE + (i * FLASH_PAGE_SIZE);
        const uint8_t *src = &ctx->buffer[i * FLASH_PAGE_SIZE];
        if (!BootCtrl_LowLevel_ProgramPage(addr, src, FLASH_PAGE_SIZE))
        {
            UART1_SendString("[RECOVERY] programming stage2 failed\r\n");
            return STATE_FAIL;
        }
    }

    /* 3. Verify (post-program byte-by-byte) */
    UART1_SendString("[RECOVERY] verifying stage2...\r\n");
    const uint8_t *flash_ptr = (const uint8_t *)STAGE2_FLASH_BASE;
    for (uint32_t i = 0; i < ctx->recv_size; ++i)
    {
        if (flash_ptr[i] != ctx->buffer[i])
        {
            UART1_SendString("[RECOVERY] verification failed\r\n");
            return STATE_FAIL;
        }
    }

    UART1_SendString("[RECOVERY] flash success\r\n");
    return STATE_DONE;
}