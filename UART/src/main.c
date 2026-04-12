// #include <stdint.h>

// /* 레지스터 절대 주소 매크로 */
// /* 레지스터 매크로 */
// #define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)
// #define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0)
// #define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)
// #define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)
// #define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)
// #define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)
// #define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)

// #define CCM_CSCDR1                              (*(volatile uint32_t *)0x400FC024) // UART 클럭 소스 선택
// #define CCM_CCGR5                               (*(volatile uint32_t *)0x400FC07C) // UART1 모듈 전원
// #define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06     (*(volatile uint32_t *)0x401F80D4) // UART1 TX 핀 설정

// #define LPUART1_BAUD                            (*(volatile uint32_t *)0x40184010) // 통신 속도 설정
// #define LPUART1_STAT                            (*(volatile uint32_t *)0x40184014) // 통신 상태 (버퍼 비었나?)
// #define LPUART1_CTRL                            (*(volatile uint32_t *)0x40184018) // TX/RX 활성화 제어
// #define LPUART1_DATA                            (*(volatile uint32_t *)0x4018401C) // 실제 데이터 쏘는 곳

// volatile uint32_t g_clock_init_marker = 0;

// /* 132MHz 클럭 설정 함수 (초안전 모드) */
// __attribute__((section(".ramfunc"), noinline))
// void Clock_Init_132MHz(void) {
//     g_clock_init_marker = 0x11111111;
//     /* 1. 안전하게 4분주 기어 장착 (AHB_PODF = 3) */
//     CCM_CBCDR &= ~(7 << 10); 
//     CCM_CBCDR |= (3 << 10);  
//     while (CCM_CDHIPR & (1 << 1)); // 변경 완료 대기

//     /* 2. 🚨 [오류 수정] CPU 클럭 소스를 528MHz Sys PLL로 변경!
//      * PRE_PERIPH_CLK_SEL(18~19비트)를 00으로 설정하여 켜져있는 Sys PLL 선택 */
//     CCM_CBCMR &= ~(3 << 18); 

//     /* 3. 우회로를 닫고 메인 경로(PERIPH_CLK_SEL = 0)로 스위칭 */
//     CCM_CBCDR &= ~(1 << 25); 
//     while (CCM_CDHIPR & (1 << 5)); // 스위칭 완료 대기
    
//     /* 최종 속도: 528MHz / 4 = 132MHz */

//     g_clock_init_marker = 0x22222222;
// }

// /* RAM 지연 함수 */
// void delay_loop(uint32_t count) {
//     for (volatile uint32_t i = 0; i < count; i++) {
//         __asm("nop"); 
//     }
// }

// /* 115200 bps 셋업 (입력 클럭 80MHz 기준) */
// void UART1_Init(void) {
//     /* 1. LPUART1 모듈 클럭 활성화 (CCGR5, CG12) */
//     CCM_CCGR5 |= (3 << 24);
    
//     /* 2. UART 클럭 소스 설정 (PLL3 80MHz, 1분주) 
//      * CSCDR1[UART_CLK_SEL] 비트 6 = 0 (PLL3_80M)
//      * CSCDR1[UART_CLK_PODF] 비트 5~0 = 0 (1분주) */
//     CCM_CSCDR1 &= ~((1 << 6) | 0x3F); 
    
//     /* 3. GPIO_AD_B0_06 핀을 LPUART1_TX 역할(ALT2)로 맵핑 */
//     IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06 = 0x02;
    
//     /* 4. UART 제어기 초기화 (안전하게 다 끄고 시작) */
//     LPUART1_CTRL = 0;
    
//     /* 5. Baudrate 설정 (115200 bps @ 80MHz)
//      * 공식: Baudrate = Clock / (OSR * SBR)
//      * SBR = 80,000,000 / (16 * 115200) = 43.4 -> 43 사용
//      * OSR = 16배수 사용 (레지스터에는 15(0xF)로 기록) */
//     LPUART1_BAUD = 0x0F000000 | 43; 
    
//     /* 6. TX 활성화 (TE 비트 = 19) */
//     LPUART1_CTRL |= (1 << 19);
// }

// /* 문자 1개 송신 함수 */
// void UART1_SendChar(char c) {
//     /* STAT 레지스터의 TDRE(Transmit Data Register Empty, 비트 23)가 1이 될 때까지 대기.
//      * 택배 기사(하드웨어)가 이전 물건을 다 싣고 출발할 때까지 기다리는 과정입니다. */
//     while (!(LPUART1_STAT & (1 << 23)));
    
//     /* 데이터 레지스터에 문자 밀어 넣기 */
//     LPUART1_DATA = c;
// }

// /* 문자열 송신 함수 */
// void UART1_SendString(const char *str) {
//     while (*str) {
//         UART1_SendChar(*str++);
//     }
// }

// /* RAM 메인 함수 */
// int main(void)
// {
//     /* 초기 지연 (디버거 접속용) */
//     delay_loop(2000000);

//     /* 🚀 132MHz로 안전하게 스위칭 */
//     Clock_Init_132MHz();        

//     /* GPIO 및 클럭 활성화 */
//     CCM_CCGR1 |= (3 << 26); 
//     IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05; 
//     GPIO1_GDIR |= (1 << 5);             

//     /* UART1 초기화 */
//     UART1_Init();
    
//     /* 시작 알림 메시지 출력 */
//     UART1_SendString("\r\n=================================\r\n");
//     UART1_SendString(" RT1020 Bare-Metal 132MHz Booted!\r\n");
//     UART1_SendString(" UART TX Initialization Success.\r\n");
//     UART1_SendString("=================================\r\n");

//     int toggle_count = 0;

//     while (1) {
//         /* LED ON */
//         GPIO1_DR |= (1 << 5);   // ON
//         UART1_SendString("LED Status: ON\r\n");
//         delay_loop(10000000);
        
//         /* LED OFF */
//         GPIO1_DR &= ~(1 << 5);  
//         UART1_SendString("LED Status: OFF\r\n");
//         delay_loop(10000000);    
        
//         toggle_count++;
//     }
//     return 0;
// }
#include <stdint.h>
#include "clock.h"
#include "uart.h"
#include "led.h"

/* 공용 딜레이 함수 */
void delay_loop(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop"); 
    }
}

int main(void)
{
    /* 초기 지연 */
    delay_loop(2000000);

    /* 모듈 초기화 */
    Clock_Init_132MHz();        
    LED_Init();
    UART1_Init();
    
    /* 부팅 메시지 전송 */
    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" UART RX Interrupt Mode Ready!\r\n");
    UART1_SendString(" Press '1' to LED ON, '0' to OFF\r\n");
    UART1_SendString("=================================\r\n");

    /* 메인 무한 루프 */
    while (1) {
        // LED_On();
        // UART1_SendString("LED Status: ON\r\n");
        // delay_loop(10000000);    
        
        // LED_Off(); 
        // UART1_SendString("LED Status: OFF\r\n");
        // delay_loop(10000000);    
        __asm("wfi"); // Wait For Interrupt: 인터럽트가 올 때까지 코어를 재워서 전력을 아낌 (선택사항)
    }
    return 0;
}