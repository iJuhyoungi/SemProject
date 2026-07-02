#include "Pwm.h"
#include "rt1020_regs.h"

void Pwm1_Init(void)
{
    // CCGR4[CG8]=bits[17:16]
    CCM_CCGR4|=(3u<<16);

    // 서브모듈 0 설정: 카운터 INIT(0) -> VAL1(주기), PWM_A 엣지 VAL2/VAL3
    FLEXPWM1_SM0CTRL=(0u<<4)|(1U<<10);          /* PRSC=÷1, FULL=1(full-cycle reload 켜야 버퍼 로드) */
    FLEXPWM1_SM0INIT=0u;
    FLEXPWM1_SM0VAL1=0xFFFFu;           // 주기 top
    FLEXPWM1_SM0VAL2=0u;                // PWM_A rising edge
    FLEXPWM1_SM0VAL3=0x8000u;           // PWM_A falling edge -> 50% duty

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

//AUTOSAR Wrapping
void Pwm_Init(const Pwm_ConfigType *ConfigPtr){
    (void)ConfigPtr;
    Pwm1_Init();
}

void Pwm_SetDutyCycle(Pwm_ChannelType Channel, uint16_t DutyCycle){
    (void)Channel;          // 레벨1: SM0 PWM_A
    // 표준 0x8000=100% -> 주기 VAL1=0xFFFF 기준 VAL3 환산
    uint16_t val3=(uint16_t)(((uint32_t)DutyCycle* 0xFFFFu)/0x8000u);
    Pwm1_SetCompare(val3);
    
}
