#include "clock.h"
#include "rt1020_regs.h"

volatile uint32_t g_clock_init_marker = 0;

/* 132MHz 클럭 설정 함수 (ITCM 대피 & noinline 최적화 방어) */
__attribute__((section(".ramfunc"), noinline))
void Clock_Init_132MHz(void) {
    g_clock_init_marker = 0x11111111;

    CCM_CBCDR |= (1 << 25); 
    while (CCM_CDHIPR & (1 << 5)); 

    CCM_CBCDR &= ~(7 << 10); 
    CCM_CBCDR |= (3 << 10);  
    while (CCM_CDHIPR & (1 << 1)); 

    CCM_CBCMR &= ~(3 << 18); 

    CCM_CBCDR &= ~(1 << 25); 
    while (CCM_CDHIPR & (1 << 5)); 
    
    g_clock_init_marker = 0x22222222;
}
