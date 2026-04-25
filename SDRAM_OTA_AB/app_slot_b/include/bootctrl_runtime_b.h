#ifndef BOOTCTRL_RUNTIME_B_H
#define BOOTCTRL_RUNTIME_B_H

#include <stdint.h>
#include "boot_policy.h"

#ifndef ENABLE_BOOTCTRL_FLASH_WRITE
#define ENABLE_BOOTCTRL_FLASH_WRITE 1
#endif

int BootCtrl_RuntimeB_Read(boot_ctrl_t *out);
int BootCtrl_RuntimeConfirm_SlotB(void);
int BootCtrl_RuntimeConfirm_SlotB_Precheck(void);
int BootCtrl_RuntimeConfirm_SlotB_Write(void);

#endif