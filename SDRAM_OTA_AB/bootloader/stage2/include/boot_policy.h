#ifndef BOOT_POLICY_H
#define BOOT_POLICY_H

#include <stdint.h>

#define BOOT_CTRL_MAGIC   0x424F4F54u   /* 'BOOT' */

#define SLOT_A            0u
#define SLOT_B            1u
#define SLOT_NONE         0xFFFFFFFFu

#define APP_SLOT_A_ADDR   0x60040000u
#define APP_SLOT_B_ADDR   0x60200000u

#define BOOT_CTRL_ADDR    0x603F0000u

#define BOOT_SUCCESS_NO   0u
#define BOOT_SUCCESS_YES  1u

#define BOOT_MAX_ATTEMPTS 1u

typedef struct {
    uint32_t magic;
    uint32_t active_slot;
    uint32_t pending_slot;
    uint32_t boot_success;
    uint32_t boot_attempts;
} boot_ctrl_t;

const boot_ctrl_t *Boot_GetCtrl(void);
uint32_t Boot_SelectSlot(const boot_ctrl_t *ctrl);
uint32_t Boot_GetSlotBase(uint32_t slot);
int Boot_IsAppValid(uint32_t app_base);
int Boot_IsPendingTrial(const boot_ctrl_t *ctrl);
uint32_t Boot_GetFallbackSlot(const boot_ctrl_t *ctrl);

#endif
