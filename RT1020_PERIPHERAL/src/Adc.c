#include "Adc.h"
#include "rt1020_regs.h"

#define ADC_GROUP0_CHANNEL 0x19u

int ADC1_Init(void)
{
    // 1) 클럭 게이트 : CCGR1[CG8] -> bits[17:16]
    CCM_CCGR1 |= (3u << 16);

    // 2) CFG: ADLPC=1(저전력), ADLSMP=1(long sample), MODE=10b(해상도), ADIV=00(÷1), ADICLK=00(IPG bus clock), ADTRG=0(SW 트리거)
    ADC1_CFG = (1u << 7) | (1u << 4) | (2u << 2);

    // 3) calibration : CG[CAL]=1 -> 완료되면 HW가 CAL비트를 clear
    ADC1_GC |= (1u << 7);
    uint32_t to = 1000000u;
    while ((ADC1_GC & (1u << 7)) && --to)
    {
    }
    return (to == 0u) ? 1 : 0;
}

uint32_t ADC1_Read(uint8_t channel)
{
    // HC0에 채널 write하면 SW trigger되어 변환이 시작됨(AIEN=0, polling)
    // HC0 write로 인하여 이전의 COCO도 자동으로 clear된다
    ADC1_HC0 = (uint32_t)(channel & 0x1Fu);

    // 변환 완료(COCO0) 대기
    uint32_t to = 1000000u;
    while (!(ADC1_HS & (1u << 0)) && --to)
    {
    }

    return ADC1_R0 & 0xFFFu; /* 12-bit 결과 (읽으면 COCO 클리어) */
}

//AUTOSAR Wrapping
void Adc_Init(const Adc_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;
    ADC1_Init();

}

Std_ReturnType Adc_ReadGroup(Adc_GroupType Group, Adc_ValueGroupType *DataBufferPtr)
{
    if(Group!=0u||DataBufferPtr==0){
        return E_NOT_OK;
    }

    DataBufferPtr[0]=(Adc_ValueGroupType)ADC1_Read(ADC_GROUP0_CHANNEL);
    return E_OK;
}

Adc_StatusType Adc_GetGroupStatus(Adc_GroupType Group)
{
    (void)Group;
    return ADC_COMPLETED;
}
