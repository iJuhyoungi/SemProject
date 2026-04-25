#include <stdint.h>
#include "boot_policy.h"

__attribute__((section(".boot_ctrl"), used))
const boot_ctrl_t g_boot_ctrl_default = {
    .magic = BOOT_CTRL_MAGIC,
    .active_slot = SLOT_A,
    .pending_slot = SLOT_NONE,
    .boot_success = BOOT_SUCCESS_NO,
    .boot_attempts = 0
};
