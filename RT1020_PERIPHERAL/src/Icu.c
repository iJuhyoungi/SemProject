#include "Icu.h"
#include "rt1020_regs.h"

/*CTRL field*/
#define CM_PRIMARY_RISING       (1u<<13)    /* CM=001: primary 상승엣지 카운트 */
#define PCS_BUSCLK_DIV8         (0xBu<<9)   /* PCS=1011: IP bus clock ÷8 */
#define PCS_CNT0_OUTPUT         (4u<<9)     /* PCS=0100: Counter 0 출력 */
#define PCS_CNT2_INPUT          (2u<<9)     /* PCS=0010: Counter 2 입력 (XBAR 에서 옴) */
#define LENGTH_REINIT           (1u<<5)     /* COMP1 도달 시 LOAD 로 재초기화 */
#define OUTMODE_TOGGLE          (3u<<0)     /* OUTMODE=011: compare 시 OFLAG 토글 */

void Icu1_Init(uint16_t gen_half_period)
{
    //클럭 게이트
    CCM_CCGR6|=(3u<<26);

    //ch0=>사각파 생성기
    TMR1_CTRL(0)=0u;
    TMR1_LOAD(0)=0u;
    TMR1_COMP1(0)=gen_half_period;      // toggle 간격
    TMR1_SCTRL(0)=0u;
    TMR1_CTRL(0)=CM_PRIMARY_RISING|PCS_BUSCLK_DIV8|LENGTH_REINIT|OUTMODE_TOGGLE;

    //ch1=>ch0출력 엣지 카운트(cascade)
    TMR1_CTRL(1)=0u;
    TMR1_CNTR(1)=0u;                    // 카운터 클리어
    TMR1_LOAD(1)=0u;
    TMR1_SCTRL(1)=0u;
    TMR1_CTRL(1)=CM_PRIMARY_RISING|PCS_CNT0_OUTPUT;

    //ch2=>PWM 트리거 펄스 카운트 (XBAR 가 TIMER2 입력으로 넣어줌 — Port_RoutePwmTrigToQtmr)
    TMR1_CTRL(2)=0u;
    TMR1_CNTR(2)=0u;
    TMR1_LOAD(2)=0u;
    TMR1_SCTRL(2)=0u;
    TMR1_CTRL(2)=CM_PRIMARY_RISING|PCS_CNT2_INPUT;
}

uint16_t Icu1_GetEdgeCount(void)
{
    return TMR1_CNTR(1);
}

uint16_t Icu1_GetPwmEdgeCount(void)
{
    return TMR1_CNTR(2);                    // ch2 = PWM 주기당 1펄스 누적
}

#include "Det.h"

static Icu_DriverStateType Icu_DriverState = ICU_DRV_UNINIT;
static const Icu_ConfigType *Icu_ConfigPtr = 0;

void Icu_Init(const Icu_ConfigType *ConfigPtr)
{
#if (ICU_DEV_ERROR_DETECT == STD_ON)
    if (ConfigPtr == 0)
    {
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_INIT, ICU_E_PARAM_POINTER);
        return;
    }
    if (Icu_DriverState == ICU_DRV_INITIALIZED)
    {
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_INIT, ICU_E_ALREADY_INITIALIZED);
        return;
    }
#endif
    Icu_ConfigPtr = ConfigPtr;
    Icu1_Init(ConfigPtr->gen_half_period);
    Icu_DriverState = ICU_DRV_INITIALIZED;
}

Icu_EdgeNumberType Icu_GetEdgeNumbers(Icu_ChannelType Channel)
{
#if (ICU_DEV_ERROR_DETECT == STD_ON)
    if (Icu_DriverState == ICU_DRV_UNINIT) {
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_GETEDGENUMBERS, ICU_E_UNINIT);
        return 0u;
    }
    if (Channel > 1u) {                     /* 채널 0=cascade, 1=PWM 루프백 */
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_GETEDGENUMBERS, ICU_E_PARAM_CHANNEL);
        return 0u;
    }
#else
    (void)Channel;
#endif
    return (Channel == 1u) ? Icu1_GetPwmEdgeCount() : Icu1_GetEdgeCount();
}

void Icu_ResetEdgeCount(Icu_ChannelType Channel)
{
#if (ICU_DEV_ERROR_DETECT == STD_ON)
    if (Icu_DriverState == ICU_DRV_UNINIT) {
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_RESETEDGECOUNT, ICU_E_UNINIT);
        return;
    }
    if (Channel > 1u) {
        Det_ReportError(ICU_MODULE_ID, 0u, ICU_SID_RESETEDGECOUNT, ICU_E_PARAM_CHANNEL);
        return;
    }
#else
    (void)Channel;
#endif
    if (Channel == 1u)
    {
        TMR1_CNTR(2) = 0u;
    }
    else
    {
        TMR1_CNTR(1) = 0u;
    }
}
