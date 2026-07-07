#include "Wdg.h"
#include "rt1020_regs.h"
#include "Det.h"

#define WDG_CS_CMD32EN (1u << 13)
#define WDG_CS_ULK (1u << 11)    // RO : unlock됨
#define WDG_CS_RCS (1u << 10)    // RO : 재설정 성공
#define WDG_CS_CLK_LPO (1u << 8) // CLK01 : 32kHz LPO
#define WDG_CS_EN (1u << 7)
#define WDG_CS_UPDATE (1u << 5) // 이후 재설정을 허용

#define WDG_REFRESH 0xB480A602u // 32bit refresh(CMD32EN=1, 단일 atomic write)
#define WDG_UNLOCK 0xD928C520u  // 32bit unlock (CMD32EN=1)

#define SRSR_WDG3 (1u << 7)

void Wdg1_Init(uint16_t timeout_ticks, uint16_t window_ticks)
{
    // 레지스터 접근용 클럭 게이트
    CCM_CCGR5 |= (3u << 4);

    __asm volatile ("cpsid i");
    
        // unlock인데 write-once 비트 재설정을 허용
        RTWDG_CNT = WDG_UNLOCK;
    while (!(RTWDG_CS & WDG_CS_ULK))
    { // ULK=1 대기
    }

    // 설정: 32kHz LPO + EN + UPDATE + 32bit cmd, window off
    RTWDG_TOVAL = timeout_ticks; // 32kHz 기준, 16-bit
    RTWDG_WIN = window_ticks;    // CS[WIN] 안 켜면 값만 준비 (현재 비활성)
    RTWDG_CS = WDG_CS_EN | WDG_CS_UPDATE | WDG_CS_CMD32EN | WDG_CS_CLK_LPO;

    // RCS=1 대기
    while (!(RTWDG_CS & WDG_CS_RCS))
    {
    }

    __asm volatile ("cpsie i");   /* 오타 수정: 재설정 끝났으니 인터럽트 복구 (cpsid→cpsie) */
}

void Wdg1_Refresh(void)
{
    RTWDG_CNT = WDG_REFRESH;
}

uint32_t Wdg1_GetCS(void)
{
    return RTWDG_CS;
}

uint32_t Wdg1_GetCNT(void)
{
    return RTWDG_CNT;
}

uint32_t Wdg1_GetTOVAL(void)
{
    return RTWDG_TOVAL;
}

static Wdg_DriverStateType Wdg_DriverState = WDG_DRV_UNINIT;
static const Wdg_ConfigType *Wdg_ConfigPtr = 0;

void Wdg_Init(const Wdg_ConfigType *ConfigPtr)
{
#if (WDG_DEV_ERROR_DETECT == STD_ON)
    if (ConfigPtr == 0) {
        Det_ReportError(WDG_MODULE_ID, 0u, WDG_SID_INIT, WDG_E_PARAM_POINTER);
        return;
    }
    if (Wdg_DriverState == WDG_DRV_INITIALIZED) {   /* Wdg 는 ALREADY_INITIALIZED 대신 DRIVER_STATE */
        Det_ReportError(WDG_MODULE_ID, 0u, WDG_SID_INIT, WDG_E_DRIVER_STATE);
        return;
    }
#endif
    Wdg_ConfigPtr = ConfigPtr;
    Wdg1_Init(ConfigPtr->timeout_ticks, ConfigPtr->window_ticks);
    Wdg_DriverState = WDG_DRV_INITIALIZED;
}

Std_ReturnType Wdg_SetMode(Wdg_ModeType Mode)
{
#if (WDG_DEV_ERROR_DETECT == STD_ON)
    if(Wdg_DriverState==WDG_DRV_UNINIT){    /* Wdg 는 E_UNINIT 대신 DRIVER_STATE */
        Det_ReportError(WDG_MODULE_ID, 0u, WDG_SID_SETMODE, WDG_E_DRIVER_STATE);
        return E_NOT_OK;
    }
    if(Mode==WDGIF_OFF_MODE){               /* RTWDOG 은 동작 중 OFF 불가 */
        Det_ReportError(WDG_MODULE_ID, 0u, WDG_SID_SETMODE, WDG_E_PARAM_MODE);
        return E_NOT_OK;
    }
#endif
    /* 레벨1: RTWDOG는 write-once라 동작 중 OFF 불가 -> OFF만 reject */
    return (Mode == WDGIF_OFF_MODE) ? E_NOT_OK : E_OK;
}

void Wdg_SetTriggerCondition(uint16_t timeout)
{
#if (WDG_DEV_ERROR_DETECT == STD_ON)
    if(Wdg_DriverState==WDG_DRV_UNINIT){    /* init 전 refresh 차단 = fail-safe 방향 */
        Det_ReportError(WDG_MODULE_ID, 0u, WDG_SID_SETTRIGGERCOND, WDG_E_DRIVER_STATE);
        return;
    }
#endif
    if(timeout>0u){
        Wdg1_Refresh();
    }
}
