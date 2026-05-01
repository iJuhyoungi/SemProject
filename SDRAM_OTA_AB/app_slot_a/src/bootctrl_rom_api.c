#include <stdint.h>
#include "bootctrl_runtime.h"
#include "flexspi_nor_config.h"

/*
 * app_slot_a 전용 low-level flash backend
 *
 * 모든 함수를 .ramfunc → ITCM 에서 실행.
 * slot 앱은 SDRAM 에서 실행되므로 strict 요건은 아니나,
 * bootloader 와 동일 모델로 일관성 + 방어적 안전 확보.
 *
 * 전제:
 * - shared/src/flexspi_nor_config.c 에 qspi_flash_config 심볼이 이미 존재함
 * - app_slot_a/linker/app_slot_a.ld 에 .ramfunc → ITCM 매핑
 * - shared/src/startup_MIMXRT1020.c 의 else 분기가 .ramfunc 를 FLASH→ITCM 복사
 */

/* ---------- ROM API type declarations ---------- */

typedef int32_t status_t;
#define kStatus_Success 0
#define BOOTCTRL_RAMFUNC __attribute__((section(".ramfunc")))


// typedef struct _flexspi_nor_config_ flexspi_nor_config_t;
// extern const flexspi_nor_config_t qspi_flash_config;

// extern const flexspi_nor_config_t qspi_flash_config;

static flexspi_nor_config_t g_bootctrl_flash_cfg_ram;
static int s_cfg_copied = 0;

volatile uint32_t g_bootctrl_dbg = 0;
volatile uint32_t g_bootctrl_dbg_status = 0;
volatile uint32_t g_bootctrl_dbg_sts0_before=0;     /* erase 직전 STS0 스냅샷 */
volatile uint32_t g_bootctrl_dbg_arbidle_wait=0;    /* erase 직전 ARBIDLE 대기 횟수 */

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
#define BOOTCTRL_FLEXSPI_AMBA_BASE      (0x60000000u)   /* FlexSPI가 바라보는 NOR flash의 주소 공간 시작점 */

  /* FlexSPI 컨트롤러 STS0 레지스터 (RM p.1503, 27.8.2.24) */                                                                                                                              
  /* base = 0x402A_8000, offset = 0xE0  → 0x402A_80E0 */ 
#define BOOTCTRL_FLEXSPI_STS0   (*(volatile uint32_t *)0x402A80E0U)
#define BOOTCTRL_STS0_ARBIDLE   (1u << 1)       /* bit[1] = ARBIDLE; 1이면 컨트롤러 idle */
#define BOOTCTRL_STS0_SEQIDLE   (1u << 0)       /* bit[0] = SEQIDLE; 1이면 sequence engine idle */

/* ---------- small helpers ---------- */

static void BootCtrl_CopyConfigToRam(void);

BOOTCTRL_RAMFUNC
static void BootCtrl_MemCpy8(void *dst, const void *src, uint32_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;

    for (uint32_t i = 0; i < size; i++) {
        d[i] = s[i];
    }
}

BOOTCTRL_RAMFUNC
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

BOOTCTRL_RAMFUNC
static void BootCtrl_RestoreIRQ(uint32_t primask)
{
    __asm volatile(
        "msr primask, %0   \n"
        :
        : "r"(primask)
        : "memory");
}

BOOTCTRL_RAMFUNC
static void BootCtrl_DsbIsb(void)
{
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

BOOTCTRL_RAMFUNC
static int BootCtrl_RomInitIfNeeded(void)
{
    static int s_inited = 0;

    if (s_inited) {
        g_bootctrl_dbg = 0x1001;
        return 1;
    }

    bootloader_api_entry_t *api = BOOTCTRL_ROM_API_TREE;
    if (!api) {
        g_bootctrl_dbg = 0xE101;
        return 0;
    }

    if (!api->flexSpiNorDriver) {
        g_bootctrl_dbg = 0xE102;
        return 0;
    }

    if (!api->flexSpiNorDriver->init) {
        g_bootctrl_dbg = 0xE103;
        return 0;
    }

    BootCtrl_CopyConfigToRam();
    g_bootctrl_dbg = 0x1002;

    status_t st = api->flexSpiNorDriver->init(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram);

    g_bootctrl_dbg_status = (uint32_t)st;

    if (st != kStatus_Success) {
        g_bootctrl_dbg = 0xE104;
        return 0;
    }

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    s_inited = 1;
    g_bootctrl_dbg = 0x1003;
    return 1;
}

/* ---------- required low-level API for bootctrl_runtime_flash.c ---------- */
BOOTCTRL_RAMFUNC
int BootCtrl_LowLevel_Read(uint32_t address, void *dst, uint32_t size)
{
    if (!dst) {
        return 0;
    }

    BootCtrl_MemCpy8(dst, (const void *)address, size);
    return 1;
}

BOOTCTRL_RAMFUNC
int BootCtrl_LowLevel_EraseSector(uint32_t address)
{
    if (address < BOOTCTRL_FLEXSPI_AMBA_BASE) {
        g_bootctrl_dbg = 0xE207;
        return 0;
    }

    if ((address % BOOTCTRL_SECTOR_SIZE) != 0u) {
        g_bootctrl_dbg = 0xE201;
        return 0;
    }

    g_bootctrl_dbg = 0x2001;

    if (!BootCtrl_RomInitIfNeeded()) {
        g_bootctrl_dbg = 0xE202;
        return 0;
    }

    bootloader_api_entry_t *api = BOOTCTRL_ROM_API_TREE;
    if (!api) {
        g_bootctrl_dbg = 0xE203;
        return 0;
    }

    if (!api->flexSpiNorDriver) {
        g_bootctrl_dbg = 0xE204;
        return 0;
    }

    if (!api->flexSpiNorDriver->erase) {
        g_bootctrl_dbg = 0xE205;
        return 0;
    }

    g_bootctrl_dbg = 0x2002;

    uint32_t primask = BootCtrl_SaveAndDisableIRQ();
    BootCtrl_DsbIsb();

    /*erase 직전 컨트롤러 상태 스냅샷*/
    g_bootctrl_dbg_sts0_before = BOOTCTRL_FLEXSPI_STS0;

    /*보험으로 AHB RX buffer 한번 더 비우기*/
    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }
    BootCtrl_DsbIsb();

    /*ARBIDLE이 1이 될 때까지 대기*/
    uint32_t wait = 0;
    while (!(BOOTCTRL_FLEXSPI_STS0 & BOOTCTRL_STS0_ARBIDLE)) {
        wait++;
        if(wait>1000000u) {
            break;
        }
        g_bootctrl_dbg_arbidle_wait = wait;
    }


    uint32_t flash_offset=address-BOOTCTRL_FLEXSPI_AMBA_BASE;
    
    status_t st = api->flexSpiNorDriver->erase(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram,
        flash_offset,
        BOOTCTRL_SECTOR_SIZE);

    g_bootctrl_dbg_status = (uint32_t)st;

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    BootCtrl_DsbIsb();
    BootCtrl_RestoreIRQ(primask);

    if (st != kStatus_Success) {
        g_bootctrl_dbg = 0xE206;
        return 0;
    }

    g_bootctrl_dbg = 0x2003;
    return 1;
}

BOOTCTRL_RAMFUNC
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

    uint32_t flash_offset=address-BOOTCTRL_FLEXSPI_AMBA_BASE;
    status_t st = api->flexSpiNorDriver->program(
        BOOTCTRL_FLEXSPI_INST,
        &g_bootctrl_flash_cfg_ram,
        flash_offset,
        (const uint32_t *)s_page_buf);

    if (api->flexSpiNorDriver->clear_cache) {
        api->flexSpiNorDriver->clear_cache(BOOTCTRL_FLEXSPI_INST);
    }

    BootCtrl_DsbIsb();
    BootCtrl_RestoreIRQ(primask);

    return (st == kStatus_Success) ? 1 : 0;
}

BOOTCTRL_RAMFUNC
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
