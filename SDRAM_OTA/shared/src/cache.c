#include <stdint.h>
#include "rt1020_regs.h"
#include "mimxrt1020.h"
#include "cache.h"

#define SCB_CCR_IC_Msk  (1UL << 17) /* I-Cache 비트 */
#define SCB_CCR_DC_Msk  (1UL << 16) /* D-Cache 비트 */


// /* 🚀 베어메탈 D-Cache 엔진 점화 함수 */
void DCache_Enable(void)
{
    uint32_t ccsidr, sets, ways;

    if (SCB_CCR & SCB_CCR_DC_Msk) return;

    SCB_CSSELR = 0;
    __asm("dsb");

    ccsidr = SCB_CCSIDR;
    sets = (ccsidr >> 13) & 0x7FFF;

    do {
        ways = (ccsidr >> 3) & 0x3FF;
        do {
            SCB_DCISW = ((ways & 0x3FF) << 30) | ((sets & 0x7FFF) << 5);
        } while (ways-- != 0);
    } while (sets-- != 0);

    __asm("dsb");

    SCB_CCR |= SCB_CCR_DC_Msk;

    __asm("dsb");
    __asm("isb");
}

void ICache_Enable(void)
{
    /* 1. 이미 I-Cache가 켜져 있다면 스킵 */
    if (SCB_CCR & SCB_CCR_IC_Msk) {
        return;
    }

    __asm("dsb");
    __asm("isb");

    /* 2. I-Cache 전체 무효화 (Invalidate)
     * 혹시 모를 쓰레기 값이 캐시에 남아있을 수 있으므로 깨끗하게 비웁니다. */
    SCB_ICIALLU = 0UL;

    /* 3. I-Cache 활성화! */
    SCB_CCR |= SCB_CCR_IC_Msk;

    __asm("dsb");
    __asm("isb");
}

/* 🚀 베어메탈 D-Cache 강제 종료 함수 */
void DCache_Disable(void)
{
    uint32_t ccsidr, sets, ways;

    if (!(SCB_CCR & SCB_CCR_DC_Msk)) return;

    SCB_CSSELR = 0;
    __asm("dsb");
    ccsidr = SCB_CCSIDR;
    sets = (ccsidr >> 13) & 0x7FFF;

    /* 끌 때는 캐시에 남은 데이터를 RAM으로 밀어내는 'Clean & Invalidate' 작업이 필수입니다. */
    do {
        ways = (ccsidr >> 3) & 0x3FF;
        do {
            SCB_DCCISW = ((ways & 0x3FF) << 30) | ((sets & 0x7FFF) << 5);
        } while (ways-- != 0);
    } while (sets-- != 0);

    __asm("dsb");
    SCB_CCR &= ~SCB_CCR_DC_Msk; /* D-Cache 스위치 OFF */
    __asm("dsb");
    __asm("isb");
}

/* 🚀 1. Cache Clean (CPU가 쓴 데이터를 SDRAM으로 밀어내기 - TX 전용) */
void DCache_CleanByAddr(void *addr, uint32_t size)
{
    uint32_t op_addr = (uint32_t)addr;
    int32_t op_size = (int32_t)size;

    /* 주소를 32바이트(캐시 라인) 경계로 내림 정렬 */
    uint32_t align_offset = op_addr & 0x1F; 
    op_addr -= align_offset;
    op_size += align_offset;

    __asm("dsb");
    /* 32바이트씩 전진하며 해당 주소의 캐시를 물리 메모리로 청소(Clean) */
    while (op_size > 0) {
        SCB_DCCMVAC = op_addr; 
        op_addr += 32;
        op_size -= 32;
    }
    __asm("dsb");
    __asm("isb");
}

/* 🚀 2. Cache Invalidate (물리 메모리의 새 데이터를 CPU 캐시로 강제 갱신 - RX 전용) */
void DCache_InvalidateByAddr(void *addr, uint32_t size)
{
    uint32_t op_addr = (uint32_t)addr;
    int32_t op_size = (int32_t)size;

    uint32_t align_offset = op_addr & 0x1F;
    op_addr -= align_offset;
    op_size += align_offset;

    __asm("dsb");
    /* 32바이트씩 전진하며 해당 주소의 캐시를 무효화(Invalidate)하여 메모리에서 다시 읽게 함 */
    while (op_size > 0) {
        SCB_DCIMVAC = op_addr;
        op_addr += 32;
        op_size -= 32;
    }
    __asm("dsb");
    __asm("isb");
}
