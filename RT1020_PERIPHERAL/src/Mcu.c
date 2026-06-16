#include "Mcu.h"
#include "lpspi.h"

/* 레벨1: 이미지가 ROM 기본 클럭(IS_BOOTLOADER) 사용 → 시스템 클럭 별도 설정 없음 */
void Mcu_Init(void)
{
}

/* 주변장치 클럭 게이트. 실무 Mcu 는 PLL·클럭트리 전체 관리 */
void Mcu_InitClock(void)
{
    LPSPI1_Clock_Enable();
}
