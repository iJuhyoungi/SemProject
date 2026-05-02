#include <stdint.h>
#include "uart.h"
#include "rt1020_regs.h"
#include "crc32.h"

/**
 * Stage 1 immutable bootloader.
 * 책임 :
 *  1. UART init
 *  2. Stage 2의 Vector Table Sanity Check(SP/PC 유효성 검사)
 *  3. Stage 2로 점프
 *
 * Stage 1은 update가 안됨  - Stage2의 buggy update가 boot chain을 깨뜨려도 Stage1이 살아있으면 recovery 경로 가능
 *
 * Stage 1 base = 0x60004000 (현재 bootloader의 LMA shift 후 위치)
 */

#define STAGE2_BASE         0x60004000u
#define RECOVERY_BASE       0x603E0000u

/* Stage 2 의 image header (vector[7..9] 활용). post_build_stage2.py 와 약속 */
#define IMG_MAGIC_OFFSET    0x1Cu
#define IMG_SIZE_OFFSET     0x20u
#define IMG_CRC_OFFSET      0x24u
#define IMG_MAGIC_VALUE     0xDEADBEEFu

/* Stage 2 영역의 size 한계 (0x60004000 ~ 0x60040000 = 240 KB) */
#define STAGE2_SIZE_MIN     0x100u
#define STAGE2_SIZE_MAX     0x3C000u

static void Jump_To_Stage2(uint32_t addr)
{
    uint32_t app_msp = *(volatile uint32_t *)addr;
    uint32_t app_pc = *(volatile uint32_t *)(addr + 4);

    __asm volatile("cpsid i"); /* 인터럽트 비활성화 */
    __asm volatile("dsb");     /* 모든 메모리 접근 완료 대기 */
    __asm volatile("isb");     /* 명령어 파이프라인 초기화 */

    SCB_VTOR = addr; /* Vector Table Offset Register에 Stage 2 벡터 테이블 주소 설정 */

    __asm volatile("dsb"); /* VTOR 설정 완료 대기 */
    __asm volatile("isb"); /* 명령어 파이프라인 초기화 */

    __asm volatile(
        "msr msp, %0   \n" /* MSP 레지스터에 Stage 2의 초기 SP 값 설정 */
        "bx %1         \n" /* Stage 2의 Reset_Handler로 점프 */
        ::"r"(app_msp),
        "r"(app_pc));
}

static int Stage2_VectorSane(uint32_t addr)
{
    uint32_t sp = *(volatile uint32_t *)addr;
    uint32_t pc = *(volatile uint32_t *)(addr + 4);

    /*SP가 DTCM/OCRAM 영역(0x2xxxxxxx) 안에 있는지*/
    if ((sp & 0xF0000000u) != 0x20000000u)
    {
        return 0;
    }
    /*PC가 FlexSPI FLASH (0x6xxxxxxx) 안에 있는지*/
    if ((pc & 0xF0000000u) != 0x60000000u)
    {
        return 0;
    }

    if ((pc & 0x1u) != 0x1u)
    {
        return 0;
    }

    return 1;
}

static int Stage2_Verify(uint32_t base)
{
    /* Vector Sanity (SP, PC, Thumb bit) */
    if (!Stage2_VectorSane(base))
    {
        UART1_SendString("[Verify] vector sanity FAIL\r\n");
        return 0;
    }

    /* Magic 검증 */
    uint32_t magic = *(volatile uint32_t *)(base + IMG_MAGIC_OFFSET);
    if (magic != IMG_MAGIC_VALUE)
    {
        UART1_SendString("[Verify] magic value FAIL\r\n");
        return 0;
    }

    /* Size 검증 */
    uint32_t size = *(volatile uint32_t *)(base + IMG_SIZE_OFFSET);
    if (size < STAGE2_SIZE_MIN || size > STAGE2_SIZE_MAX)
    {
        UART1_SendString("[Verify] image size FAIL\r\n");
        return 0;
    }

    /* CRC 검증 */
    uint32_t expected_crc = *(volatile uint32_t *)(base + IMG_CRC_OFFSET);
    uint32_t computed_crc = CRC32_ComputeWithSkip(
        (const uint8_t *)base, size, IMG_CRC_OFFSET);

    if (expected_crc != computed_crc)
    {
        UART1_SendString("[Verify] CRC FAIL\r\n");
        return 0;
    }

    return 1;
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[BL1] Stage 1 immutable\r\n");
    UART1_SendString("\r\n=============================\r\n");

    UART1_SendString("[BL1] Verifying Stage 2 ...\r\n");
    if (Stage2_Verify(STAGE2_BASE))
    {
        UART1_SendString("[BL1] Stage 2 OK - jumping to 0x60004000\r\n");
        Jump_To_Stage2(STAGE2_BASE);
    }

    UART1_SendString("[BL1] Stage 2 INVALID - jumping to recovery (0x603E0000)\r\n");
    Jump_To_Stage2(RECOVERY_BASE);

    /*unreachable*/
    for (;;)
    {
        __asm("wfi");
    }

    return 0;
}