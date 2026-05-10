#ifndef RECOVERY_PROTOCOL_H                                                                                                                                                              
#define RECOVERY_PROTOCOL_H  

#include <stdint.h>

#define MAX_STAGE2_SIZE 0x3C000u /* 240 KB */

/* Protocol magic strings (docs/PROTOCOL_RECOVERY_UART.md 참고) */
#define MSG_READY "READY!\r\n"
#define MSG_ACK "ACK!\r\n"
#define MSG_TOOBIG "TOOBIG!\r\n"
#define MSG_CRCOK "CRCOK!\r\n"
#define MSG_CRCFAIL "CRCFAIL!\r\n"
#define MSG_DONE "DONE!\r\n"
#define MSG_FLASHFAIL "FLASHFAIL!\r\n"

/* State Machine */
typedef enum
{
    STATE_IDLE,
    STATE_HANDSHAKE,
    STATE_RECV_SIZE,
    STATE_RECV_DATA,
    STATE_RECV_CRC,
    STATE_VERIFY_CRC,
    STATE_FLASH,
    STATE_FAIL,
    STATE_DONE
} recovery_state_t;

/* Protocol context - handler 간 공유 state */
typedef struct {
    uint32_t recv_size;
    uint32_t recv_crc;
    uint8_t *buffer;            /* OCRAM 의 __recovery_buffer_start__ */
}recovery_ctx_t;

/* State handlers — 각 handler 가 next state 반환 */                                                                                                                                     
recovery_state_t Recovery_HandleIdle(recovery_ctx_t *ctx);                                                                                                                               
recovery_state_t Recovery_HandleHandshake(recovery_ctx_t *ctx);                                                                                                                          
recovery_state_t Recovery_HandleRecvSize(recovery_ctx_t *ctx);                                                                                                                           
recovery_state_t Recovery_HandleRecvData(recovery_ctx_t *ctx);                                                                                                                           
recovery_state_t Recovery_HandleRecvCRC(recovery_ctx_t *ctx);                                                                                                                            
recovery_state_t Recovery_HandleVerifyCRC(recovery_ctx_t *ctx);
recovery_state_t Recovery_HandleFlash(recovery_ctx_t *ctx); 

#endif