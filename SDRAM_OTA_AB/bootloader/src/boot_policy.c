#include "boot_policy.h"

static uint32_t read_u32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

const boot_ctrl_t *Boot_GetCtrl(void)
{
    return (const boot_ctrl_t *)BOOT_CTRL_ADDR;
}

uint32_t Boot_GetSlotBase(uint32_t slot)
{
    return (slot == SLOT_B) ? APP_SLOT_B_ADDR : APP_SLOT_A_ADDR;
}

int Boot_IsAppValid(uint32_t app_base)
{
    uint32_t msp = read_u32(app_base + 0);
    uint32_t pc  = read_u32(app_base + 4);

    if (msp == 0xFFFFFFFFu || pc == 0xFFFFFFFFu) return 0;
    if ((pc & 1u) == 0u) return 0;
    return 1;
}

int Boot_IsPendingTrial(const boot_ctrl_t *ctrl)
{
    if (ctrl->magic != BOOT_CTRL_MAGIC) return 0;
    if (ctrl->pending_slot != SLOT_A && ctrl->pending_slot != SLOT_B) return 0;
    return 1;
}

uint32_t Boot_GetFallbackSlot(const boot_ctrl_t *ctrl)
{
    if (ctrl->magic != BOOT_CTRL_MAGIC) {
        return SLOT_A;
    }
    return (ctrl->active_slot == SLOT_B) ? SLOT_B : SLOT_A;
}

uint32_t Boot_SelectSlot(const boot_ctrl_t *ctrl)
{
    if (ctrl->magic != BOOT_CTRL_MAGIC) {
        return SLOT_A;
    }

    if (Boot_IsPendingTrial(ctrl)) {
        if (ctrl->boot_success == BOOT_SUCCESS_NO &&
            ctrl->boot_attempts < BOOT_MAX_ATTEMPTS) {
            return ctrl->pending_slot;
        }

        /* trial 실패 또는 횟수 초과 → active로 rollback */
        return Boot_GetFallbackSlot(ctrl);
    }

    return Boot_GetFallbackSlot(ctrl);
}

// uint32_t Boot_SelectSlot(const boot_ctrl_t *ctrl)
// {
//     (void)ctrl;
//     return SLOT_B;
// }
