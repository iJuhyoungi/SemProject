#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "rt1020_regs.h"
#include "crc32.h"
#include "recovery_protocol.h"

/* OCRAM buffer (linker 가 export 한 symbol) */
extern uint8_t __recovery_buffer_start__[];
extern uint8_t __recovery_buffer_end__[];

static void halt_with_led(void)
{
    /* LED 빠른 깜빡 — passive recovery (느린 깜빡) 와 구분 */
    while (1) {                                                                                                                                                                          
        LED_On();
        for (volatile uint32_t i = 0; i < 1000000; i++) { __asm("nop"); }                                                                                                                
        LED_Off();                                                                                                                                                                       
        for (volatile uint32_t i = 0; i < 1000000; i++) { __asm("nop"); }
    }
}

static void nvic_reset(void) {
    UART1_SendString("[RECOVERY] System Resetting...\r\n");
    __asm volatile ("dsb");
    SCB_AIRCR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    __asm volatile ("dsb");
    while(1) {

    }
}

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; ++i)
    {
        __asm("nop");
    }
}

int main(void)
{
    UART1_SendString("\r\n========================================\r\n");
    UART1_SendString(" [RECOVERY] active mode (Step 4)\r\n");                                                                                                                            
    UART1_SendString("========================================\r\n");


    LED_Init();

    recovery_ctx_t ctx={
        .recv_size=0,
        .recv_crc=0,
        .buffer=__recovery_buffer_start__,
    };

    recovery_state_t state = STATE_IDLE;

    while(state!=STATE_DONE && state != STATE_FAIL) {
        switch(state) {
            case STATE_IDLE:
                state = Recovery_HandleIdle(&ctx);
                break;
            case STATE_HANDSHAKE:
                state = Recovery_HandleHandshake(&ctx);
                break;
            case STATE_RECV_SIZE:
                state = Recovery_HandleRecvSize(&ctx);
                break;
            case STATE_RECV_DATA:
                state = Recovery_HandleRecvData(&ctx);
                break;
            case STATE_RECV_CRC:
                state = Recovery_HandleRecvCRC(&ctx);
                break;
            case STATE_VERIFY_CRC:
                state = Recovery_HandleVerifyCRC(&ctx);
                break;
            case STATE_FLASH:
                state = Recovery_HandleFlash(&ctx);
                break;
            default:
                state = STATE_FAIL;
                break;
        }
    }

    if(state==STATE_DONE) {
        UART1_SendString(MSG_DONE);
        nvic_reset();
    }

    UART1_SendString(MSG_FLASHFAIL);
    halt_with_led();

    
    return 0;
}