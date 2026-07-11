#include "WdgM.h"
#include "Wdg.h"
#include "Det.h"

static const WdgM_ConfigType *WdgM_ConfigPtr = 0;

static volatile uint16_t WdgM_AliveCounter = 0;
static volatile uint8_t WdgM_FailCount = 0;
static volatile WdgM_LocalStatusType WdgM_Status = 0;
static volatile uint8_t WdgM_FirstCycle = 1;

void WdgM_Init(const WdgM_ConfigType *ConfigPtr)
{
#if (WDGM_DEV_ERROR_DETECT == STD_ON)
    if (ConfigPtr == 0)
    {
        Det_ReportError(WDGM_MODULE_ID, 0u, WDGM_SID_INIT, WDGM_E_PARAM_CONFIG);
        return;
    }
#endif
    WdgM_ConfigPtr = ConfigPtr;
    WdgM_AliveCounter = 0;
    WdgM_FailCount = 0;
    WdgM_Status = WDGM_LOCAL_STATUS_OK;
    WdgM_FirstCycle = 1;
}

Std_ReturnType WdgM_CheckpointReached(WdgM_SupervisedEntityIdType SEID, WdgM_CheckpointIdType CheckpointID)
{
    (void)CheckpointID;
#if (WDGM_DEV_ERROR_DETECT == STD_ON)
    if(WdgM_ConfigPtr==0){
        Det_ReportError(WDGM_MODULE_ID, 0u, WDGM_SID_CHECKPOINTREACHED, WDGM_E_NO_INIT);
        return E_NOT_OK;
    }
    if(SEID!=0){
        Det_ReportError(WDGM_MODULE_ID, 0u, WDGM_SID_CHECKPOINTREACHED, WDGM_E_PARAM_SEID);
        return E_NOT_OK;
    }
#endif
    __asm volatile ("cpsid i");
    WdgM_AliveCounter++;
    __asm volatile ("cpsie i");
    return E_OK;
}

/* GPT tick(1초) ISR 컨텍스트에서 호출 — 규칙: UART/Det 금지, 짧게 */
void WdgM_MainFunction(void)
{
    if (WdgM_ConfigPtr == 0) {
        return;                     /* init 전: 스펙은 DET 신고지만 ISR 규칙상 침묵 (레벨1 타협) */
    }

    if (WdgM_FirstCycle) {
        WdgM_FirstCycle = 0;
        WdgM_AliveCounter = 0;
        return;
    }

    uint16_t cnt = WdgM_AliveCounter;
    WdgM_AliveCounter = 0;          /* 다음 판정 창 시작 */

    if (WdgM_Status == WDGM_LOCAL_STATUS_EXPIRED) {
        Wdg_SetTriggerCondition(0u);        /* EXPIRED 는 편도 — 굶김 유지, 리셋으로만 복구 */
        return;
    }

    if ((cnt >= WdgM_ConfigPtr->expected_min) && (cnt <= WdgM_ConfigPtr->expected_max)) {
        WdgM_Status = WDGM_LOCAL_STATUS_OK; /* 정상 창: 회복 + 먹임 */
        WdgM_FailCount = 0;
        Wdg_SetTriggerCondition(100u);
    } else {
        WdgM_FailCount++;                   /* 위반 창: hang(미달) 또는 폭주(초과) */
        if (WdgM_FailCount > WdgM_ConfigPtr->failed_tolerance) {
            WdgM_Status = WDGM_LOCAL_STATUS_EXPIRED;
            Wdg_SetTriggerCondition(0u);    /* 굶김 시작 -> TOVAL(2.048s) 후 HW 리셋 */
        } else {
            WdgM_Status = WDGM_LOCAL_STATUS_FAILED;
            Wdg_SetTriggerCondition(100u);  /* 아직 tolerance 안 — 먹이면서 지켜봄 */
        }
    }
}

Std_ReturnType WdgM_GetGlobalStatus(WdgM_GlobalStatusType *Status)
{
#if (WDGM_DEV_ERROR_DETECT == STD_ON)
    if (WdgM_ConfigPtr == 0) {
        Det_ReportError(WDGM_MODULE_ID, 0u, WDGM_SID_GETGLOBALSTATUS, WDGM_E_NO_INIT);
        return E_NOT_OK;
    }
    if (Status == 0) {
        Det_ReportError(WDGM_MODULE_ID, 0u, WDGM_SID_GETGLOBALSTATUS, WDGM_E_PARAM_POINTER);
        return E_NOT_OK;
    }
#endif
    *Status = WdgM_Status;
    return E_OK;
}

