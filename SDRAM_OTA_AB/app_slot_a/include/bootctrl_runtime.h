#ifndef BOOTCTRL_RUNTIME_H
#define BOOTCTRL_RUNTIME_H

#include <stdint.h>
#include "boot_policy.h"

/*
 * 안전장치:
 * 0 = 구조만 활성, 실제 flash write 비활성
 * 1 = 실제 erase/program/verify 활성
 */
#ifndef ENABLE_BOOTCTRL_FLASH_WRITE
#define ENABLE_BOOTCTRL_FLASH_WRITE 1
#endif

int BootCtrl_Runtime_Read(boot_ctrl_t *out);
int BootCtrl_RuntimeConfirm_SlotA(void);

/* Phase B */
int BootCtrl_RuntimeConfirm_SlotA_Precheck(void);
int BootCtrl_RuntimeConfirm_SlotA_Write(void);

#endif

