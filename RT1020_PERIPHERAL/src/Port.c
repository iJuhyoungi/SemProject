#include "Port.h"
#include "lpspi.h"
#include "rt1020_regs.h"

/* LPSPI1 핀(SCK/PCS0/SDO/SDI) IOMUXC mux. 실무 Port 는 전체 핀 config 테이블 적용 */
void Port_Init(void)
{
    // LPSPI1_Clock_Enable();
    LPSPI1_Pin_Init();
}

/* PWM SM0 트리거를 QuadTimer1 ch2 입력으로 잇는 칩 내부 경로 (docs/ICU_LOOP_NOTES.md).
   신호 라우팅이므로 Port 소관 — 실무 MCAL 도 XBAR 설정은 Port 가 맡는다 */
void Port_RoutePwmTrigToQtmr(void)
{
    CCM_CCGR2 |= (3u << 22);                            /* CG11: xbar1 클럭 게이트 ON */
    XBARA1_SEL44 = (XBARA1_SEL44 & ~0x7Fu) | 40u;       /* OUT88(QTIMER1_TIMER2) <- IN40(SM0 TRIG0|1) */
    IOMUXC_GPR_GPR6 |= (1u <<2);                        /* TIMER2 입력을 pad 대신 XBAR 로 */
}

