#include "bootctrl_runtime_boot.h"

extern int BootCtrl_BootloaderWrite_Precheck(const boot_ctrl_t *current);
extern int BootCtrl_BootloaderWrite_Internal(void);

int BootCtrl_MarkTrialAttempt(const boot_ctrl_t *current)
{
    if (!BootCtrl_BootloaderWrite_Precheck(current)) {
        return 0;
    }

    return BootCtrl_BootloaderWrite_Internal();
}