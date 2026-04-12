#include "uart.h"
#include "rt1020_regs.h"
#include "led.h" // LED 제어를 위해 추가

void UART1_Init(void) {
    CCM_CCGR5 |= (3 << 24);
    CCM_CSCDR1 &= ~((1 << 6) | 0x3F); 
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06 = 0x02;     //ALT2 — Select mux mode: ALT2 mux port: LPUART1_TX of instance: lpuart1
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07 = 0x02;     //ALT2 — Select mux mode: ALT2 mux port: LPUART1_RX of instance: lpuart1

    LPUART1_CTRL = 0;
    LPUART1_BAUD = 0x0F000000 | 43; 
    LPUART1_CTRL |= (1 << 18) |(1 << 19) | (1 << 21);

    NVIC_ISER0 |= (1 << 20);
}

void UART1_SendChar(char c) {
    while (!(LPUART1_STAT & (1 << 23)));
    LPUART1_DATA = c;
}

void UART1_SendString(const char *str) {
    while (*str) {
        UART1_SendChar(*str++);
    }
}

/* 🚀 LPUART1 인터럽트 서비스 루틴 (데이터가 수신될 때마다 하드웨어가 자동 실행) */
void LPUART1_IRQHandler(void) {
    /* 수신 버퍼에 데이터가 있는지(RDRF 비트 21) 확인 */
    if (LPUART1_STAT & (1 << 21)) {
        /* 데이터를 읽는 순간 하드웨어가 자동으로 RDRF 플래그를 지워줍니다 */
        char rx_data = LPUART1_DATA; 
        
        /* 터미널에 에코(메아리) 전송 */
        UART1_SendChar(rx_data);
        
        /* 재미있는 테스트: 키보드 입력으로 LED 제어 */
        if (rx_data == '1') {
            LED_On();
            UART1_SendString(" -> LED ON!\r\n");
        } else if (rx_data == '0') {
            LED_Off();
            UART1_SendString(" -> LED OFF!\r\n");
        }
    }
}
