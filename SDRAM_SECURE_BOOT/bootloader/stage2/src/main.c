#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "bignum.h"
#include "verify.h"
#include "embedded_pubkey.h"

#define APP_A_BASE  0x60048000u
#define APP_B_BASE  0x60088000u

static void halt_on_fail(void){
    LED_On();
    __asm volatile("cpsid i");
    while(1){
        __asm volatile("wfi");
    }
}

/**
 * Stage 2 — Secure Boot 의 검증 대상 image (demo payload).
 *
 * 이 binary 는 "Stage 1 이 무엇을 검증하는가" 의 대상일 뿐, 하는 일은
 * 최소화한다. 호스트 서명 도구(Step 3)가 이 .bin 을 서명해 첨부하고,
 * Stage 1 이 검증 통과 시 여기로 점프한다.
 *
 * LED 깜빡임 = 정상 실행 신호 (Stage 1 halt 시의 상시 점등과 구분).
 */

static void delay_loop(uint32_t count)
{ 
    for (volatile uint32_t i = 0; i < count; ++i)
    {
        __asm("nop");
    }
}

static void print_hex32(uint32_t v)
{
    static const char hex[]="0123456789abcdef";
    char buf[2]={0,};
    UART1_SendString("0x");
    for(int i=7;i>=0;--i){
        buf[0]=hex[(v>>(i*4))&0xF];
        UART1_SendString(buf);
    }
}

// image header의 version 슬롯을 읽는 코드, erased flash면 0xFFFFFFFF
static uint32_t read_version(uint32_t base)
{
    return *(volatile uint32_t*)(base+IMG_VERSION_OFFSET);
}

int main(void)
{
    UART1_SendString("\r\n-----------------------------\r\n");
    UART1_SendString("[BL2] Stage 2 verified & running\r\n");
    UART1_SendString("-----------------------------\r\n");

    LED_Init();

    /*A/B version check*/
    uint32_t va=read_version(APP_A_BASE);
    uint32_t vb=read_version(APP_B_BASE);
    UART1_SendString("[BL2] App A version: ");
    print_hex32(va);
    UART1_SendString("\r\n");

    UART1_SendString("[BL2] App B version: ");
    print_hex32(vb);
    UART1_SendString("\r\n");

    /*priority check*/
    uint32_t primary, secondary;
    const char *prim_name, *sec_name;
    if(vb>va){
        primary=APP_B_BASE;
        prim_name="App B";
        secondary=APP_A_BASE;
        sec_name="App A";
    }else{
        primary=APP_A_BASE;
        prim_name="App A";
        secondary=APP_B_BASE;
        sec_name="App B";
    }

    /* primary 시도 */
    UART1_SendString("[BL2] Trying "); UART1_SendString(prim_name);
    UART1_SendString(" (higher version) ...\r\n");
    if (verify_image(primary, EMBEDDED_PUBKEY_MODULUS)) {
        UART1_SendString("[BL2] "); UART1_SendString(prim_name);
        UART1_SendString(" OK - jumping\r\n");
        jump_to_image(primary);
    }

    /* fallback */
    UART1_SendString("[BL2] "); UART1_SendString(prim_name);
    UART1_SendString(" FAIL - falling back to "); UART1_SendString(sec_name);
    UART1_SendString(" ...\r\n");
    if (verify_image(secondary, EMBEDDED_PUBKEY_MODULUS)) {
        UART1_SendString("[BL2] "); UART1_SendString(sec_name);
        UART1_SendString(" OK - jumping\r\n");
        jump_to_image(secondary);
    }

    /* 둘 다 FAIL */ 
    UART1_SendString("[BL2] Both A and B INVALID - halting\r\n");

    halt_on_fail();

    for (;;) { __asm("wfi"); }
    return 0;

}
