#ifndef BOOTCTRL_RUNTIME_H
#define BOOTCTRL_RUNTIME_H

#include <stdint.h>
#include "boot_policy.h"

int BootCtrl_Runtime_Read(boot_ctrl_t *out);
int BootCtrl_RuntimeConfirm_SlotA(void);

/* Phase B */
int BootCtrl_RuntimeConfirm_SlotA_Precheck(void);
int BootCtrl_RuntimeConfirm_SlotA_Write(void);

#endif

