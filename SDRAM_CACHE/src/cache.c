#include <stdint.h>
#include "rt1020_regs.h"
#include "mimxrt1020.h"
#include "cache.h"

#define SCB_CCR_IC_Msk  (1UL << 17) /* I-Cache 비트 */
#define SCB_CCR_DC_Msk  (1UL << 16) /* D-Cache 비트 */

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

/* 🚀 베어메탈 D-Cache 엔진 점화 함수 */
void DCache_Enable(void)
{
    uint32_t ccsidr, sets, ways;

    /* 1. 이미 켜져 있다면 패스 */
    if (SCB_CCR & SCB_CCR_DC_Msk) return;

    /* 2. Level 1 데이터 캐시 선택 */
    SCB_CSSELR = 0;
    __asm("dsb");

    /* 3. 캐시 하드웨어 스펙(크기) 읽어오기 */
    ccsidr = SCB_CCSIDR;

    /* 비트 연산을 통해 바둑판의 Set 개수와 Way 개수 추출 */
    sets = (ccsidr >> 13) & 0x7FFF;
    
    /* 4. 이중 루프를 돌면서 모든 Set과 Way를 무효화(Invalidate) */
    do {
        ways = (ccsidr >> 3) & 0x3FF;
        do {
            /* Cortex-M7 기준: Way는 30비트 위치, Set은 5비트 위치로 시프트하여 레지스터에 기록 */
            SCB_DCISW = ((ways & 0x3FF) << 30) | ((sets & 0x7FFF) << 5);
        } while (ways-- != 0);
    } while (sets-- != 0);

    __asm("dsb");

    /* 5. 초기화가 끝났으므로 당당하게 D-Cache 스위치 ON! */
    SCB_CCR |= SCB_CCR_DC_Msk;

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
