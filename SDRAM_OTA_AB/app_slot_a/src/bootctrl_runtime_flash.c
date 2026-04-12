#include <stdint.h>
#include "bootctrl_runtime.h"

/*
 * Phase B - Safe draft
 *
 * 아직 실제 FlexSPI NOR erase/program은 하지 않는다.
 * 대신:
 * - write 대상 boot_ctrl_t를 구성
 * - 최소 precheck 수행
 * - 실제 write 진입 함수 골격만 만든다
 *
 * 다음 단계에서만 아래 stub를 진짜 구현으로 교체한다.
 */

static boot_ctrl_t g_bootctrl_shadow;

static int BootCtrl_RuntimeBuildConfirmImage_SlotA(boot_ctrl_t *out)
{
    if (!out) {
        return 0;
    }

    out->magic = BOOT_CTRL_MAGIC;
    out->active_slot = SLOT_A;
    out->pending_slot = SLOT_NONE;
    out->boot_success = BOOT_SUCCESS_YES;
    out->boot_attempts = 0;

    return 1;
}

int BootCtrl_RuntimeConfirm_SlotA_Precheck(void)
{
    boot_ctrl_t current;

    if (!BootCtrl_Runtime_Read(&current)) {
        return 0;
    }

    if (current.magic != BOOT_CTRL_MAGIC) {
        return 0;
    }

    if (!BootCtrl_RuntimeBuildConfirmImage_SlotA(&g_bootctrl_shadow)) {
        return 0;
    }

    return 1;
}

/*
 * 다음 단계에서 이 두 함수를 진짜 FlexSPI NOR sector erase / page program으로 교체
 */
__attribute__((section(".ramfunc")))
static int BootCtrl_FlashErase_SectorStub(uint32_t address)
{
    (void)address;
    return 0;
}

__attribute__((section(".ramfunc")))
static int BootCtrl_FlashProgram_Stub(uint32_t address, const void *src, uint32_t size)
{
    (void)address;
    (void)src;
    (void)size;
    return 0;
}

int BootCtrl_RuntimeConfirm_SlotA_Write(void)
{
    /*
     * 현재 단계:
     * - 실제 write는 하지 않음
     * - 아래 stub 호출 구조만 둠
     * - 실패 반환이 정상
     *
     * 다음 단계에서만:
     * - sector erase
     * - page program
     * - read-back verify
     * 를 붙인다.
     */

    if (!BootCtrl_FlashErase_SectorStub(BOOT_CTRL_ADDR)) {
        return 0;
    }

    if (!BootCtrl_FlashProgram_Stub(BOOT_CTRL_ADDR, &g_bootctrl_shadow, sizeof(g_bootctrl_shadow))) {
        return 0;
    }

    return 1;
}


