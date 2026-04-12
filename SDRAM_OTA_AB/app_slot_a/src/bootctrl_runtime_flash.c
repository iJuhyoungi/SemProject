#include <stdint.h>
#include "bootctrl_runtime.h"


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

 /*
 * 일반적인 NOR flash 가정값.
 * 사용하는 flash가 다르면 여기 값만 조정하면 됨.
 */
#define BOOTCTRL_FLASH_SECTOR_SIZE   4096u
#define BOOTCTRL_FLASH_PAGE_SIZE      256u

/*
 * boot control은 한 sector 안에 있다고 가정.
 * BOOT_CTRL_ADDR가 sector 시작 주소라는 현재 구조를 전제로 함.
 */
#define BOOTCTRL_FLASH_BASE_ADDR   (BOOT_CTRL_ADDR)

/*
 * shadow buffer:
 * sector erase/program 시 whole-sector write가 필요할 수 있으므로
 * sector 크기만큼 shadow를 둔다.
 *
 * 주의:
 * 현재 구조에선 boot control만 해당 sector를 사용한다고 가정.
 * 나중에 같은 sector에 다른 데이터가 섞이면 read-modify-write 필요.
 */

static uint8_t g_bootctrl_sector_shadow[BOOTCTRL_FLASH_SECTOR_SIZE];
static boot_ctrl_t g_bootctrl_confirm_image;

/*
 * ===== 실제 저수준 NOR 함수 연결 포인트 =====
 *
 * 아래 3개는 나중에 실제 FlexSPI NOR driver에 연결해야 한다.
 * 지금은 ENABLE_BOOTCTRL_FLASH_WRITE == 0 이면 사용되지 않는다.
 *
 * 함수 계약:
 * - erase: sector erase 성공 시 1, 실패 시 0
 * - program: 지정 주소에 size 바이트 program 성공 시 1, 실패 시 0
 * - read: 지정 주소에서 size 바이트 memcpy 수준으로 읽기 성공 시 1, 실패 시 0
 *
 * 중요:
 * - erase/program 함수는 .ramfunc 로 동작해야 한다.
 * - read는 XIP 직접 읽기로 충분하면 일반 함수여도 된다.
 */

#if ENABLE_BOOTCTRL_FLASH_WRITE
extern int BootCtrl_LowLevel_Read(uint32_t address, void *dst, uint32_t size);
extern int BootCtrl_LowLevel_EraseSector(uint32_t address);
extern int BootCtrl_LowLevel_ProgramPage(uint32_t address, const void *src, uint32_t size);
#endif

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

    if (!BootCtrl_RuntimeBuildConfirmImage_SlotA(&g_bootctrl_confirm_image)) {
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
    /*
     * 현재는 같은 sector 전체를 다 우리가 소유한다고 가정하지만,
     * 안전하게 current flash 내용을 먼저 읽고 앞부분만 덮어쓴다.
     */
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
            chunk = (size - offset);
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

int BootCtrl_RuntimeConfirm_SlotA_Write(void)
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

