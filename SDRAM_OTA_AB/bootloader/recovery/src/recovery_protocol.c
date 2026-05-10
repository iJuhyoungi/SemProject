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
static uint32_t read_uint32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] 
         | ((uint32_t)bytes[1] << 8) 
         | ((uint32_t)bytes[2] << 16) 
         | ((uint32_t)bytes[3] << 24);
}

recovery_state_t Recovery_HandleIdle(recovery_ctx_t *ctx)                                                                                                                                
{                                                        
    (void)ctx;                                                                                                                                                                           
    UART1_SendString("[RECOVERY] waiting for handshake...\r\n");
                                                                                                                                                                                        
    /* Phase 1: HOST → BOARD: "RECV?\n" (6 bytes ASCII) */
    uint8_t buf[6];                                                                                                                                                                      
    UART1_RxBytes(buf, 6);                                
                                                                                                                                                                                        
    if (buf[0] == 'R' && buf[1] == 'E' && buf[2] == 'C' &&                                                                                                                               
        buf[3] == 'V' && buf[4] == '?' && buf[5] == '\n') {
        return STATE_HANDSHAKE;                                                                                                                                                          
    }                                                                                                                                                                                    
    
    UART1_SendString("[RECOVERY] unexpected handshake, retry\r\n");                                                                                                                      
    return STATE_IDLE;   /* match 실패 시 IDLE 재진입 (host 재시도 가능) */                                                                                                              
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
                                                                                                                                                                                        
    if (size == 0u || size > MAX_STAGE2_SIZE) {           
        UART1_SendString(MSG_TOOBIG);                                                                                                                                                    
        return STATE_IDLE;   /* host 재시도 가능 */
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
                                                                                                                                                                                        
    if (computed != ctx->recv_crc) {                      
        UART1_SendString(MSG_CRCFAIL);                                                                                                                                                   
        return STATE_IDLE;   /* host 재시도 (transmission 오류) */
    }                                                                                                                                                                                    
                                                                                                                                                                                        
    UART1_SendString(MSG_CRCOK);
    return STATE_FLASH;                                                                                                                                                                  
}

recovery_state_t Recovery_HandleFlash(recovery_ctx_t *ctx)
{                                                         
    (void)ctx;                                                                                                                                                                           
    /* TODO Step 4-B-4: erase + program + post-program verify */
    UART1_SendString("[RECOVERY] STATE_FLASH (placeholder, Step 4-B-4)\r\n");                                                                                                            
    return STATE_DONE;                                                       
}