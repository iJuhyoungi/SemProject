#include "Icu.h"
#include "rt1020_regs.h"

/*CTRL field*/
#define CM_PRIMARY_RISING       (1u<<13)    /* CM=001: primary 상승엣지 카운트 */
#define PCS_BUSCLK_DIV8         (0xBu<<9)   /* PCS=1011: IP bus clock ÷8 */
#define PCS_CNT0_OUTPUT         (4u<<9)     /* PCS=0100: Counter 0 출력 */
#define LENGTH_REINIT           (1u<<5)     /* COMP1 도달 시 LOAD 로 재초기화 */
#define OUTMODE_TOGGLE          (3u<<0)     /* OUTMODE=011: compare 시 OFLAG 토글 */

void Icu1_Init(void)
{
    //클럭 게이트
    CCM_CCGR6|=(3u<<26);

    //ch0=>사각파 생성기
    TMR1_CTRL(0)=0u;
    TMR1_LOAD(0)=0u;
    TMR1_COMP1(0)=0x0FFFu;              // toggle 간격
    TMR1_SCTRL(0)=0u;
    TMR1_CTRL(0)=CM_PRIMARY_RISING|PCS_BUSCLK_DIV8|LENGTH_REINIT|OUTMODE_TOGGLE;

    //ch1=>ch0출력 엣지 카운트(cascade)
    TMR1_CTRL(1)=0u;
    TMR1_CNTR(1)=0u;                    // 카운터 클리어
    TMR1_LOAD(1)=0u;
    TMR1_SCTRL(1)=0u;
    TMR1_CTRL(1)=CM_PRIMARY_RISING|PCS_CNT0_OUTPUT;
}

uint16_t Icu1_GetEdgeCount(void)
{
    return TMR1_CNTR(1);
}

void Icu_Init(const Icu_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    Icu1_Init();
}

Icu_EdgeNumberType Icu_GetEdgeNumbers(Icu_ChannelType Channel)
{
    (void)Channel;
    return Icu1_GetEdgeCount();         //CNTR1=ch0 누적 엣지 수
}

void Icu_ResetEdgeCount(Icu_ChannelType Channel)
{
    (void)Channel;
    TMR1_CNTR(1)=0u;                    // ch1 카운터 클리어
}
