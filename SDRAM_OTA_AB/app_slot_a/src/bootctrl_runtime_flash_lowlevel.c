#include <stdint.h>
#include "bootctrl_runtime.h"
#include "flexspi_nor_config.h"

/*
 * app_slot_a 전용 low-level flash backend
 *
 * - shared/ 미수정
 * - bootloader/ 미수정
 * - app_slot_b/ 미수정
 *
 * 전제:
 * - shared/src/flexspi_nor_config.c 에 qspi_flash_config 심볼이 이미 존재함
 * - ENABLE_BOOTCTRL_FLASH_WRITE == 1 일 때만 상위 write 경로가 이 파일을 호출함
 */

/* ---------- ROM API type declarations ---------- */

typedef int32_t status_t;
#define kStatus_Success 0


// typedef struct _flexspi_nor_config_ flexspi_nor_config_t;
// extern const flexspi_nor_config_t qspi_flash_config;

// extern const flexspi_nor_config_t qspi_flash_config;

static flexspi_nor_config_t g_bootctrl_flash_cfg_ram;
static int s_cfg_copied = 0;

typedef struct _flexspi_nor_driver_interface
{
    uint32_t version;
    status_t (*init)(uint32_t instance, flexspi_nor_config_t *config);
    status_t (*program)(uint32_t instance, flexspi_nor_config_t *config, uint32_t dstAddr, const uint32_t *src);
    status_t (*erase_all)(uint32_t instance, flexspi_nor_config_t *config);
    status_t (*erase)(uint32_t instance, flexspi_nor_config_t *config, uint32_t start, uint32_t lengthInBytes);
    status_t (*read)(uint32_t instance, flexspi_nor_config_t *config, uint32_t *dst, uint32_t addr, uint32_t lengthInBytes);
    void (*clear_cache)(uint32_t instance);
    status_t (*xfer)(uint32_t instance, void *xfer);
    status_t (*update_lut)(uint32_t instance, uint32_t seqIndex, const uint32_t *lutBase, uint32_t seqNumber);
} flexspi_nor_driver_interface_t;

typedef struct _bootloader_api_entry
{
    const uint32_t version;
    const char *copyright;
    void (*runBootloader)(void *arg);
    const uint32_t *reserved0;
    const flexspi_nor_driver_interface_t *flexSpiNorDriver;
    const uint32_t *reserved1;
    const uint32_t *reserved2;
} bootloader_api_entry_t;

#define BOOTCTRL_ROM_API_TREE   (*(bootloader_api_entry_t **)(0x0020001cU))
#define BOOTCTRL_FLEXSPI_INST   (0u)
#define BOOTCTRL_SECTOR_SIZE    (4096u)
#define BOOTCTRL_PAGE_SIZE      (256u)

/* ---------- small helpers ---------- */

static void BootCtrl_CopyConfigToRam(void);

static void BootCtrl_MemCpy8(void *dst, const void *src, uint32_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

static uint32_t BootCtrl_SaveAndDisableIRQ(void)
{
    uint32_t primask;
    __asm volatile(
        "mrs %0, primask   \n"
        "cpsid i           \n"
        : "=r"(primask)
        :
        : "memory");
    return primask;
}

static void BootCtrl_RestoreIRQ(uint32_t primask)
{
    __asm volatile(
        "msr primask, %0   \n"
        :
        : "r"(primask)
        : "memory");
}

static void BootCtrl_DsbIsb(void)
{
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

static int BootCtrl_RomInitIfNeeded(void)
{
    static int s_inited = 0;

    if (s_inited) {
        return 1;
    }

    bootloader_api_entry_t *api = BOOTCTRL_ROM_API_TREE;
    if (!api) {
        return 0;
    }

    if (!api->flexSpiNorDriver) {
        return 0;
    }

    if (!api->flexSpiNorDriver->init) {
        return 0;
    }

    BootCtrl_CopyConfigToRam();

    status_t st = api->flexSpiNorDriver->init(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram);

    if (st != kStatus_Success) {
        return 0;
    }

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    s_inited = 1;
    return 1;
}

/* ---------- required low-level API for bootctrl_runtime_flash.c ---------- */

int BootCtrl_LowLevel_Read(uint32_t address, void *dst, uint32_t size)
{
    if (!dst) {
        return 0;
    }

    BootCtrl_MemCpy8(dst, (const void *)address, size);
    return 1;
}

int BootCtrl_LowLevel_EraseSector(uint32_t address)
{
    if ((address % BOOTCTRL_SECTOR_SIZE) != 0u) {
        return 0;
    }

    if (!BootCtrl_RomInitIfNeeded()) {
        return 0;
    }

    bootloader_api_entry_t *api = BOOTCTRL_ROM_API_TREE;
    if (!api || !api->flexSpiNorDriver || !api->flexSpiNorDriver->erase) {
        return 0;
    }

    uint32_t primask = BootCtrl_SaveAndDisableIRQ();
    BootCtrl_DsbIsb();

    status_t st = api->flexSpiNorDriver->erase(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram,
        address,
        BOOTCTRL_SECTOR_SIZE);

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    BootCtrl_DsbIsb();
    BootCtrl_RestoreIRQ(primask);

    return (st == kStatus_Success) ? 1 : 0;
}

int BootCtrl_LowLevel_ProgramPage(uint32_t address, const void *src, uint32_t size)
{
    static uint8_t s_page_buf[BOOTCTRL_PAGE_SIZE];

    if (!src) {
        return 0;
    }

    if ((address % BOOTCTRL_PAGE_SIZE) != 0u) {
        return 0;
    }

    /*
     * 현재 상위 코드가 sector shadow(4096B)를 256B page 단위로 호출하므로
     * size는 항상 256이 정상이다.
     */
    if (size != BOOTCTRL_PAGE_SIZE) {
        return 0;
    }

    if (!BootCtrl_RomInitIfNeeded()) {
        return 0;
    }

    bootloader_api_entry_t *api = BOOTCTRL_ROM_API_TREE;
    if (!api || !api->flexSpiNorDriver || !api->flexSpiNorDriver->program) {
        return 0;
    }

    BootCtrl_MemCpy8(s_page_buf, src, BOOTCTRL_PAGE_SIZE);

    uint32_t primask = BootCtrl_SaveAndDisableIRQ();
    BootCtrl_DsbIsb();

    status_t st = api->flexSpiNorDriver->program(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram,
        address,
        (const uint32_t *)s_page_buf);

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    BootCtrl_DsbIsb();
    BootCtrl_RestoreIRQ(primask);

    return (st == kStatus_Success) ? 1 : 0;
}

static void BootCtrl_CopyConfigToRam(void)
{
    if (s_cfg_copied) {
        return;
    }

    BootCtrl_MemCpy8(&g_bootctrl_flash_cfg_ram,
                     &qspi_flash_config,
                     sizeof(g_bootctrl_flash_cfg_ram));
    s_cfg_copied = 1;
}
