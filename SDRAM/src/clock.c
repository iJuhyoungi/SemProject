/*
 * clock.c
 * -----------------------------------------------------------------------------
 * 시스템 클럭(Clock Tree)을 132MHz 로 설정한다.
 *
 * 주의: 이 파일이 하는 일은 "clock gating(모듈 ON/OFF)"이 아니라
 *       "clock tree(주파수) 설정"이다. 즉 소스(PLL) 선택 + 분주비 설정.
 *       모듈별 ON/OFF 인 clock gating 은 CCGR 레지스터로, 각 드라이버에서 수행한다.
 *
 * 목표 주파수: Sys PLL(528MHz) / 4 = 132MHz
 * -----------------------------------------------------------------------------
 */
#include "clock.h"
#include "rt1020_regs.h"

/* clock.c 자체 레지스터 매크로 (rt1020_regs.h 와 별개로 이 파일에서 직접 정의).
 * 모두 CCM(Clock Controller Module) 영역의 레지스터다. */
#define CCM_CCGR1                               (*(volatile uint32_t *)0x400FC06C) /* Clock Gating Reg 1 */
#define IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B0_05     (*(volatile uint32_t *)0x401F80D0) /* 핀 기능(MUX) 설정 */
#define GPIO1_GDIR                              (*(volatile uint32_t *)0x401B8004) /* GPIO1 입출력 방향 */
#define GPIO1_DR                                (*(volatile uint32_t *)0x401B8000) /* GPIO1 데이터 레지스터 */
#define CCM_CBCMR                               (*(volatile uint32_t *)0x400FC018) /* Bus Clock Mux  — 소스 선택 */
#define CCM_CBCDR                               (*(volatile uint32_t *)0x400FC014) /* Bus Clock Div  — 분주/경로 */
#define CCM_CDHIPR                              (*(volatile uint32_t *)0x400FC048) /* Div Handshake  — 변경 완료 상태 */

/* 클럭 초기화 진행 단계를 표시하는 마커.
 * 디버거로 이 변수를 들여다보면 함수가 어디까지 실행됐는지 확인할 수 있다. */
volatile uint32_t g_clock_init_marker = 0;

/* 132MHz 클럭 설정 함수.
 * .ramfunc 섹션 배치: 클럭을 바꾸는 동안 코드가 Flash(XIP)에 있으면 위험하므로
 *                     RAM 에서 실행되도록 한다.
 * noinline: 컴파일러가 이 함수를 인라인 펼쳐서 .ramfunc 배치를 깨뜨리지 못하게 막는다. */
__attribute__((section(".ramfunc"), noinline))
void Clock_Init_132MHz(void) {
    g_clock_init_marker = 0x11111111;   /* [단계 1] 진입 표시 */

    /* 1. 우회로로 임시 전환 — 소스를 바꾸기 전 글리치 방지.
     *    CBCDR[25] = PERIPH_CLK_SEL. 1로 두면 periph_clk2(임시/안전 경로)를 사용. */
    CCM_CBCDR |= (1 << 25);
    while (CCM_CDHIPR & (1 << 5));       /* CDHIPR[5] PERIPH_CLK_SEL_BUSY 가 풀릴 때까지 대기 */

    /* 2. AHB 분주비를 4분주로 설정.
     *    CBCDR[12:10] = AHB_PODF. 먼저 필드를 clear(~(7<<10)) 후 3을 써넣음.
     *    PODF 는 (값+1) 분주 -> 3 이면 4분주. */
    CCM_CBCDR &= ~(7 << 10);
    CCM_CBCDR |= (3 << 10);
    while (CCM_CDHIPR & (1 << 1));       /* CDHIPR[1] AHB_PODF_BUSY 대기 */

    /* 3. 시스템 클럭 소스를 Sys PLL(528MHz)로 선택.
     *    CBCMR[19:18] = PRE_PERIPH_CLK_SEL. 00 으로 두면 Sys PLL 선택. */
    CCM_CBCMR &= ~(3 << 18);

    /* 4. 우회로를 닫고 메인 경로로 복귀.
     *    CBCDR[25] = 0 -> PERIPH_CLK_SEL 메인 경로(pre_periph_clk) 사용. */
    CCM_CBCDR &= ~(1 << 25);
    while (CCM_CDHIPR & (1 << 5));       /* 스위칭 완료 대기 */

    /* 최종 결과: 528MHz / 4 = 132MHz */

    g_clock_init_marker = 0x22222222;   /* [단계 2] 완료 표시 */
}
