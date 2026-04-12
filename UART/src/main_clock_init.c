#include <stdint.h>

/* 레지스터 절대 주소 매크로 */
/* 레지스터 매크로 */
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0)
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)

volatile uint32_t g_clock_init_marker = 0;

/* 132MHz 클럭 설정 함수 (초안전 모드) */
__attribute__((section(".ramfunc"), noinline))
void Clock_Init_132MHz(void) {
    g_clock_init_marker = 0x11111111;
    /* 1. 안전하게 4분주 기어 장착 (AHB_PODF = 3) */
    CCM_CBCDR &= ~(7 << 10); 
    CCM_CBCDR |= (3 << 10);  
    while (CCM_CDHIPR & (1 << 1)); // 변경 완료 대기

    /* 2. 🚨 [오류 수정] CPU 클럭 소스를 528MHz Sys PLL로 변경!
     * PRE_PERIPH_CLK_SEL(18~19비트)를 00으로 설정하여 켜져있는 Sys PLL 선택 */
    CCM_CBCMR &= ~(3 << 18); 

    /* 3. 우회로를 닫고 메인 경로(PERIPH_CLK_SEL = 0)로 스위칭 */
    CCM_CBCDR &= ~(1 << 25); 
    while (CCM_CDHIPR & (1 << 5)); // 스위칭 완료 대기
    
    /* 최종 속도: 528MHz / 4 = 132MHz */

    g_clock_init_marker = 0x22222222;
}

/* RAM 지연 함수 */
void delay_loop(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop"); 
    }
}

/* RAM 메인 함수 */
int main(void)
{
    /* 초기 지연 (디버거 접속용) */
    delay_loop(2000000);

    /* 🚀 132MHz로 안전하게 스위칭 */
    Clock_Init_132MHz();        

    /* GPIO 및 클럭 활성화 */
    CCM_CCGR1 |= (3 << 26); 
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05; 
    GPIO1_GDIR |= (1 << 5);             

    while (1) {
        GPIO1_DR |= (1 << 5);   // ON
        delay_loop(500000);    
        
        GPIO1_DR &= ~(1 << 5);  // OFF
        delay_loop(500000);    
    }
    return 0;
}
