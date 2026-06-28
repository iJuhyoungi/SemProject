#include "Wdg.h"
#include "rt1020_regs.h"

#define WDG_CS_CMD32EN (1u << 13)
#define WDG_CS_ULK (1u << 11)    // RO : unlock됨
#define WDG_CS_RCS (1u << 10)    // RO : 재설정 성공
#define WDG_CS_CLK_LPO (1u << 8) // CLK01 : 32kHz LPO
#define WDG_CS_EN (1u << 7)
#define WDG_CS_UPDATE (1u << 5) // 이후 재설정을 허용

#define WDG_REFRESH 0xB480A602u // 32bit refresh(CMD32EN=1, 단일 atomic write)
#define WDG_UNLOCK 0xD928C520u  // 32bit unlock (CMD32EN=1)

#define SRSR_WDG3 (1u << 7)

void Wdg1_Init(uint32_t timeout_ticks)
{
    // 레지스터 접근용 클럭 게이트
    CCM_CCGR5 |= (3u << 4);

        // unlock인데 write-once 비트 재설정을 허용
        RTWDG_CNT = WDG_UNLOCK;
    while (!(RTWDG_CS & WDG_CS_ULK))
    { // ULK=1 대기
    }

    // 설정: 32kHz LPO + EN + UPDATE + 32bit cmd, window off
    RTWDG_TOVAL = timeout_ticks; // 32kHz 기준(0x18000)
    RTWDG_WIN = 0u;
    RTWDG_CS = WDG_CS_EN | WDG_CS_UPDATE | WDG_CS_CMD32EN | WDG_CS_CLK_LPO;

    // RCS=1 대기
    while (!(RTWDG_CS & WDG_CS_RCS))
    {
    }
}

void Wdg1_Refresh(void)
{
    RTWDG_CNT = WDG_REFRESH;
}

uint32_t Wdg1_GetCS(void)
{
    return RTWDG_CS;
}

uint32_t Wdg_GetResetCause(void)
{
    return SRC_SRSR;
}

void Wdg_ClearResetCause(void)
{
    SRC_SRSR = SRC_SRSR;
}

uint32_t Wdg1_GetCNT(void)
{
    return RTWDG_CNT;
}

uint32_t Wdg1_GetTOVAL(void)
{
    return RTWDG_TOVAL;
}
