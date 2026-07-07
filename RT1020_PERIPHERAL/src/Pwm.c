#include "Pwm.h"
#include "rt1020_regs.h"
#include "Det.h"

static Pwm_DriverStateType Pwm_DriverState = PWM_DRV_UNINIT;
static const Pwm_ConfigType *Pwm_ConfigPtr = 0;

void Pwm1_Init(uint16_t period, uint16_t initial_val3)
{
    // CCGR4[CG8]=bits[17:16]
    CCM_CCGR4|=(3u<<16);

    // 서브모듈 0 설정: 카운터 INIT(0) -> VAL1(주기), PWM_A 엣지 VAL2/VAL3
    FLEXPWM1_SM0CTRL=(0u<<4)|(1U<<10);          /* PRSC=÷1, FULL=1(full-cycle reload 켜야 버퍼 로드) */
    FLEXPWM1_SM0INIT=0u;
    FLEXPWM1_SM0VAL1=period;            // 주기 top
    FLEXPWM1_SM0VAL2=0u;                // PWM_A rising edge
    FLEXPWM1_SM0VAL3=initial_val3;      // PWM_A falling edge -> 50% duty

    // 출력 enable. pin의 경우 EMC라서 관측에는 scope가 필요
    FLEXPWM1_OUTEN|=(1u<<8);

    // 버퍼 로드 + clock start
    FLEXPWM1_MCTRL|=(1u<<0);            // LDOK SM0: 버퍼된 INIT/VAL 반영
    FLEXPWM1_MCTRL|=(1u<<8);            // RUN SM0: 서브모듈 클럭 시작 → 카운터·파형 동작
}

uint16_t Pwm1_GetCount(void)
{
    return FLEXPWM1_SM0CNT;
}

//helper
static void Pwm1_SetCompare(uint16_t val3){
    FLEXPWM1_SM0VAL3=val3;
    FLEXPWM1_MCTRL|=(1u<<0);
}

static uint16_t Pwm_DutyToVal3(uint16_t duty, uint16_t period){
    return (uint16_t)(((uint32_t)duty * period) / 0x8000u);
}


//AUTOSAR Wrapping
void Pwm_Init(const Pwm_ConfigType *ConfigPtr)
{
#if (PWM_DEV_ERROR_DETECT == STD_ON)
    if (ConfigPtr == 0) {
        Det_ReportError(PWM_MODULE_ID, 0u, PWM_SID_INIT, PWM_E_PARAM_POINTER);
        return;
    }
    if (Pwm_DriverState == PWM_DRV_INITIALIZED) {
        Det_ReportError(PWM_MODULE_ID, 0u, PWM_SID_INIT, PWM_E_ALREADY_INITIALIZED);
        return;
    }
#endif
    Pwm_ConfigPtr = ConfigPtr;
    Pwm1_Init(ConfigPtr->period, Pwm_DutyToVal3(ConfigPtr->initial_duty, ConfigPtr->period));
    Pwm_DriverState = PWM_DRV_INITIALIZED;
}

void Pwm_SetDutyCycle(Pwm_ChannelType Channel, uint16_t DutyCycle){
#if (PWM_DEV_ERROR_DETECT == STD_ON)
    if (Pwm_DriverState == PWM_DRV_UNINIT) {
        Det_ReportError(PWM_MODULE_ID, 0u, PWM_SID_SETDUTYCYCLE, PWM_E_UNINIT);
        return;
    }
    if (Channel != 0u) {
        Det_ReportError(PWM_MODULE_ID, 0u, PWM_SID_SETDUTYCYCLE, PWM_E_PARAM_CHANNEL);
        return;
    }
#endif

    // 표준 0x8000=100% -> 주기 VAL1=0xFFFF 기준 VAL3 환산
    // uint16_t val3=(uint16_t)(((uint32_t)DutyCycle* 0xFFFFu)/0x8000u);
    Pwm1_SetCompare(Pwm_DutyToVal3(DutyCycle, Pwm_ConfigPtr->period));
    
}
