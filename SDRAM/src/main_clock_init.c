/*
 * main_clock_init.c
 * -----------------------------------------------------------------------------
 * [초기 학습용 단독 프로토타입 — 현재 빌드에는 미포함]
 *
 * "클럭을 132MHz 로 올린 뒤 LED 를 깜빡인다"를 단일 파일로 연습한 버전.
 * 이후 clock.c / led.c / main.c 로 모듈화되었다.
 * main() 이 중복 정의되므로 main.c 와 동시에 빌드할 수 없다 — 학습 기록용으로 보존.
 * -----------------------------------------------------------------------------
 */
#include <stdint.h>

/* 레지스터 절대 주소 매크로 (모두 CCM / IOMUXC / GPIO 영역). */
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C)  /* Clock Gating Reg 1 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0)  /* 핀 기능(MUX) 설정 */
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004)  /* GPIO1 입출력 방향 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000)  /* GPIO1 데이터 레지스터 */
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018)  /* Bus Clock Mux — 소스 선택 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014)  /* Bus Clock Div — 분주/경로 */
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048)  /* Div Handshake — 변경 완료 상태 */

/* 클럭 초기화 진행 단계 마커 (디버거로 진행도 확인용). */
volatile uint32_t g_clock_init_marker = 0;

/* 132MHz 클럭 설정 함수 (초안전 모드).
 * .ramfunc: 클럭 변경 중에는 RAM 에서 실행 / noinline: 인라인 펼침 방지. */
__attribute__((section(".ramfunc"), noinline))
void Clock_Init_132MHz(void) {
    g_clock_init_marker = 0x11111111;
    /* 1. 안전하게 4분주 기어 장착 (AHB_PODF = 3)
     *    CBCDR[12:10] 필드를 clear 후 3 설정 -> (3+1)=4분주. */
    CCM_CBCDR &= ~(7 << 10);
    CCM_CBCDR |= (3 << 10);
    while (CCM_CDHIPR & (1 << 1)); // 변경 완료 대기 (AHB_PODF_BUSY)

    /* 2. 🚨 [오류 수정] CPU 클럭 소스를 528MHz Sys PLL로 변경!
     * PRE_PERIPH_CLK_SEL(18~19비트)를 00으로 설정하여 켜져있는 Sys PLL 선택 */
    CCM_CBCMR &= ~(3 << 18);

    /* 3. 우회로를 닫고 메인 경로(PERIPH_CLK_SEL = 0)로 스위칭 */
    CCM_CBCDR &= ~(1 << 25);
    while (CCM_CDHIPR & (1 << 5)); // 스위칭 완료 대기 (PERIPH_CLK_SEL_BUSY)

    /* 최종 속도: 528MHz / 4 = 132MHz */

    g_clock_init_marker = 0x22222222;
}

/* RAM 지연 함수 — volatile 카운터로 컴파일러의 루프 제거 최적화를 방지. */
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
    CCM_CCGR1 |= (3 << 26);                     /* GPIO1 모듈 클럭 ON (CCGR1 CG13) */
    IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05 = 0x05; /* 핀을 ALT5(GPIO1_IO05)로 */
    GPIO1_GDIR |= (1 << 5);                     /* IO05 를 출력으로 */

    while (1) {
        GPIO1_DR |= (1 << 5);   // ON
        delay_loop(500000);

        GPIO1_DR &= ~(1 << 5);  // OFF
        delay_loop(500000);
    }
    return 0;
}
