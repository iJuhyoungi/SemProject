#include "bootctrl_runtime.h"

int BootCtrl_Runtime_Read(boot_ctrl_t *out)
{
    if (!out) {
        return 0;
    }

    const boot_ctrl_t *src = (const boot_ctrl_t *)BOOT_CTRL_ADDR;
    *out = *src;
    return 1;
}

/* 실제 구현은 bootctrl_runtime_flash.c */
int BootCtrl_RuntimeConfirm_SlotA_Precheck(void);
int BootCtrl_RuntimeConfirm_SlotA_Write(void);

int BootCtrl_RuntimeConfirm_SlotA(void)
{
    if (!BootCtrl_RuntimeConfirm_SlotA_Precheck()) {
        return 0;
    }

    return BootCtrl_RuntimeConfirm_SlotA_Write();
}

