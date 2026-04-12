#include "led.h"
#include "rt1020_regs.h"

void LED_Init(void) {
    CCM_CCGR1 |= (3 << 26); 
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05; 
    GPIO1_GDIR |= (1 << 5); 
}

void LED_On(void) {
    // GPIO1_DR |= (1 << 5);
    GPIO1_DR &= ~(1 << 5);
}

void LED_Off(void) {
    // GPIO1_DR &= ~(1 << 5);
    GPIO1_DR |= (1 << 5);
}
