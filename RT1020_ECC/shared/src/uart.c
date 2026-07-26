#include "uart.h"
#include "rt1020_regs.h"

void UART1_Init(void) {
    CCM_CCGR5 |= (3 << 24);
    CCM_CSCDR1 &= ~((1 << 6) | 0x3F);
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_06 = 0x02;
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_07 = 0x02;

    LPUART1_CTRL = 0;
    LPUART1_BAUD = 0x0F000000 | 43;

    /* polling-only */
    LPUART1_CTRL |= (1 << 18) | (1 << 19);
}

void UART1_SendChar(char c) {
    while (!(LPUART1_STAT & (1 << 23))) {}
    LPUART1_DATA = c;
}

void UART1_SendString(const char *str) {
    while (*str) {
        UART1_SendChar(*str++);
    }
}

void UART1_SendHex32(uint32_t val) {
    char buffer[11];
    buffer[0] = '0';
    buffer[1] = 'x';
    buffer[10] = '\0';

    for (int j = 0; j < 8; j++) {
        uint32_t nibble = (val >> ((7 - j) * 4)) & 0x0F;
        buffer[2 + j] = (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
    }
    UART1_SendString(buffer);
}

void LPUART1_IRQHandler(void)
{
    /* C-1에서는 사용 안 함 */
    while (LPUART1_STAT & (1 << 21)) {
        volatile uint32_t dummy = LPUART1_DATA;
        (void)dummy;
    }
}

/*                                                                                                                                                                                       
* RX byte (blocking polling).                            
*                                                                                                                                                                                       
* RDRF (Receive Data Register Full, STAT bit 21) polling.
* UART1_Init() 이 LPUART1_CTRL.RE (bit 18) 를 set 하므로 RX 자동 활성.                                                                                                                  
*                                                                                                                                                                                       
* Recovery 의 active mode (Phase 4 Step 4) 에서 처음 사용 — host 와 byte 단위 교환.                                                                                                     
* IRQ-driven 아닌 polling 인 이유: recovery 의 main 이 다른 일 안 함 (UART RX 만).                                                                                                      
*/                                                                                                                                                                                      
uint8_t UART1_RxByte(void)                                                                                                                                                               
{                                                                                                                                                                                        
    while (!(LPUART1_STAT & (1u << 21))) {                
        /* wait until RX FIFO has data (RDRF bit) */                                                                                                                                     
    }                                                                                                                                                                                    
    return (uint8_t)(LPUART1_DATA & 0xFFu);
}                                                                                                                                                                                        
                                                        
void UART1_RxBytes(uint8_t *buf, uint32_t n)                                                                                                                                             
{
    for (uint32_t i = 0; i < n; i++) {                                                                                                                                                   
        buf[i] = UART1_RxByte();                          
    }
}