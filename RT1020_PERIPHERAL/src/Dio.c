#include "Dio.h"
#include "led.h"

/* 레벨1 : 단일 채널 = 보드 LED. 실무 Dio는 채널/포트 테이블 */
void Dio_Init(void)
{
    LED_Init();
}

void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level)
{
    (void)ChannelId;
    if (Level == STD_HIGH)
        LED_On();
    else
        LED_Off();
}
