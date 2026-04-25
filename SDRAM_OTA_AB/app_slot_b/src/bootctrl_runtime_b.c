#include "bootctrl_runtime_b.h"

int BootCtrl_RuntimeB_Read(boot_ctrl_t *out)
{
    if (!out) {
        return 0;
    }

    const boot_ctrl_t *src = (const boot_ctrl_t *)BOOT_CTRL_ADDR;
    *out = *src;
    return 1;
}

int BootCtrl_RuntimeConfirm_SlotB_Precheck(void);
int BootCtrl_RuntimeConfirm_SlotB_Write(void);

int BootCtrl_RuntimeConfirm_SlotB(void)
{
    if (!BootCtrl_RuntimeConfirm_SlotB_Precheck()) {
        return 0;
    }

    return BootCtrl_RuntimeConfirm_SlotB_Write();
}