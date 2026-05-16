/*
 * led.c
 * -----------------------------------------------------------------------------
 * 보드 LED 제어 (GPIO1_IO05 핀).
 *
 * 이 보드의 LED 는 active-low 결선이다:
 *   핀 LOW  -> LED 켜짐
 *   핀 HIGH -> LED 꺼짐
 * 따라서 LED_On 이 비트를 0으로, LED_Off 가 비트를 1로 만든다.
 * -----------------------------------------------------------------------------
 */
#include "led.h"
#include "rt1020_regs.h"

/* LED 핀을 사용할 수 있도록 클럭/핀/방향을 초기화한다. */
void LED_Init(void) {
    /* 1. Clock gating: GPIO1 모듈 클럭 ON.
     *    CCGR1 의 CG13(비트 27:26)을 11(=3)로 설정. 이게 없으면 GPIO 레지스터가 죽어있다. */
    CCM_CCGR1 |= (3 << 26);

    /* 2. 핀 MUX: GPIO_AD_B0_05 핀을 ALT5 기능으로 -> GPIO1_IO05.
     *    MUX 레지스터에 모드 번호 5 를 써넣는다. */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05;                 // ALT5 — Select mux mode: ALT5 mux port: GPIO1_IO05 of instance: gpio1

    /* 3. 방향 설정: GDIR 의 비트5 = 1 -> IO05 를 출력으로. */
    GPIO1_GDIR |= (1 << 5);                                     // GPIO1 모듈의 IO05 핀을 출력으로 설정
}

/* LED 켜기 — active-low 이므로 핀을 LOW(비트 클리어)로. */
void LED_On(void) {
    // GPIO1_DR |= (1 << 5);          // (참고) active-high 보드였다면 이 줄을 사용
    GPIO1_DR &= ~(1 << 5);            // DR[5] = 0 -> 핀 LOW -> LED ON
}

/* LED 끄기 — 핀을 HIGH(비트 셋)로. */
void LED_Off(void) {
    // GPIO1_DR &= ~(1 << 5);         // (참고) active-high 보드였다면 이 줄을 사용
    GPIO1_DR |= (1 << 5);             // DR[5] = 1 -> 핀 HIGH -> LED OFF
}
