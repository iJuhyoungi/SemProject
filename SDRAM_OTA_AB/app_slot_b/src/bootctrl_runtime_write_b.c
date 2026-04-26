#include <stdint.h>
#include "bootctrl_runtime_b.h"

static void BootCtrl_MemSet(void *dst, uint8_t value, uint32_t size)
{
    volatile uint8_t *p = (volatile uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) {
        p[i] = value;
    }
}

static void BootCtrl_MemCpy(void *dst, const void *src, uint32_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;
    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

#define BOOTCTRL_FLASH_SECTOR_SIZE   4096u
#define BOOTCTRL_FLASH_PAGE_SIZE      256u
#define BOOTCTRL_FLASH_BASE_ADDR      (BOOT_CTRL_ADDR)

static uint8_t g_bootctrl_sector_shadow[BOOTCTRL_FLASH_SECTOR_SIZE];
static boot_ctrl_t g_bootctrl_confirm_image;

#if ENABLE_BOOTCTRL_FLASH_WRITE
extern int BootCtrl_LowLevel_Read(uint32_t address, void *dst, uint32_t size);
extern int BootCtrl_LowLevel_EraseSector(uint32_t address);
extern int BootCtrl_LowLevel_ProgramPage(uint32_t address, const void *src, uint32_t size);
#endif

static int BootCtrl_RuntimeBuildConfirmImage_SlotB(boot_ctrl_t *out)
{
    if (!out) {
        return 0;
    }

    out->magic = BOOT_CTRL_MAGIC;
    out->active_slot = SLOT_B;
    out->pending_slot = SLOT_NONE;
    out->boot_success = BOOT_SUCCESS_YES;
    out->boot_attempts = 0;

    return 1;
}

int BootCtrl_RuntimeConfirm_SlotB_Precheck(void)
{
    boot_ctrl_t current;

    if (!BootCtrl_RuntimeB_Read(&current)) {
        return 0;
    }

    if (current.magic != BOOT_CTRL_MAGIC) {
        return 0;
    }

    /*
     * Slot B는 trial boot로 들어온 경우에만 confirm한다.
     * 이미 active B인 경우에는 불필요하게 flash write하지 않는다.
     */
    if (current.pending_slot != SLOT_B) {
        return 0;
    }

    if (current.boot_success != BOOT_SUCCESS_NO) {
        return 0;
    }

    if (!BootCtrl_RuntimeBuildConfirmImage_SlotB(&g_bootctrl_confirm_image)) {
        return 0;
    }

    return 1;
}

static int BootCtrl_Verify(const void *expected, uint32_t size)
{
    const uint8_t *flash_ptr = (const uint8_t *)BOOTCTRL_FLASH_BASE_ADDR;
    const uint8_t *exp = (const uint8_t *)expected;

    for (uint32_t i = 0; i < size; i++) {
        if (flash_ptr[i] != exp[i]) {
            return 0;
        }
    }

    return 1;
}

#if ENABLE_BOOTCTRL_FLASH_WRITE

static int BootCtrl_PrepareSectorShadow(void)
{
    if (!BootCtrl_LowLevel_Read(BOOTCTRL_FLASH_BASE_ADDR,
                                g_bootctrl_sector_shadow,
                                BOOTCTRL_FLASH_SECTOR_SIZE)) {
        return 0;
    }

    BootCtrl_MemSet(g_bootctrl_sector_shadow, 0xFF, BOOTCTRL_FLASH_SECTOR_SIZE);
    BootCtrl_MemCpy(g_bootctrl_sector_shadow,
                    &g_bootctrl_confirm_image,
                    sizeof(g_bootctrl_confirm_image));

    return 1;
}

__attribute__((section(".ramfunc")))
static int BootCtrl_FlashErase_Sector(uint32_t address)
{
    return BootCtrl_LowLevel_EraseSector(address);
}

__attribute__((section(".ramfunc")))
static int BootCtrl_FlashProgram_Buffer(uint32_t address, const uint8_t *src, uint32_t size)
{
    uint32_t offset = 0;

    while (offset < size) {
        uint32_t chunk = BOOTCTRL_FLASH_PAGE_SIZE;
        if ((size - offset) < chunk) {
            chunk = size - offset;
        }

        if (!BootCtrl_LowLevel_ProgramPage(address + offset, src + offset, chunk)) {
            return 0;
        }

        offset += chunk;
    }

    return 1;
}

#else

static int BootCtrl_PrepareSectorShadow(void)
{
    BootCtrl_MemSet(g_bootctrl_sector_shadow, 0xFF, BOOTCTRL_FLASH_SECTOR_SIZE);
    BootCtrl_MemCpy(g_bootctrl_sector_shadow,
                    &g_bootctrl_confirm_image,
                    sizeof(g_bootctrl_confirm_image));
    return 1;
}

__attribute__((section(".ramfunc")))
static int BootCtrl_FlashErase_Sector(uint32_t address)
{
    (void)address;
    return 0;
}

__attribute__((section(".ramfunc")))
static int BootCtrl_FlashProgram_Buffer(uint32_t address, const uint8_t *src, uint32_t size)
{
    (void)address;
    (void)src;
    (void)size;
    return 0;
}

#endif

int BootCtrl_RuntimeConfirm_SlotB_Write(void)
{
    if (!BootCtrl_PrepareSectorShadow()) {
        return 0;
    }

    if (!BootCtrl_FlashErase_Sector(BOOTCTRL_FLASH_BASE_ADDR)) {
        return 0;
    }

    if (!BootCtrl_FlashProgram_Buffer(BOOTCTRL_FLASH_BASE_ADDR,
                                      g_bootctrl_sector_shadow,
                                      BOOTCTRL_FLASH_SECTOR_SIZE)) {
        return 0;
    }

    if (!BootCtrl_Verify(&g_bootctrl_confirm_image, sizeof(g_bootctrl_confirm_image))) {
        return 0;
    }

    return 1;
}