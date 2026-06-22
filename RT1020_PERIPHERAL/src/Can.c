#include "Can.h"
#include "rt1020_regs.h"

int Can1_Init(void)
{
    // PE 클럭 소스 = osc 24MHz (게이트 off -> mux 변경 -> on)
    CCM_CCGR0 &= ~((3u << 14) | (3u << 16));                            /* CG7·CG8 off */
    CCM_CSCMR2 = (CCM_CSCMR2 & ~((3u << 8) | (0xFu << 2))) | (1u << 8); /* CAN_CLK_SEL=01(osc), PODF=0(÷1) */
    CCM_CCGR0 |= (3u << 14) | (3u << 16);                               /* CG7(ipg)·CG8(PE) on */

    /*모듈 enable*/
    FLEXCAN1_MCR &= ~(1u << 31);

    /*soft reset*/
    FLEXCAN1_MCR |= (1u << 25);
    uint32_t to = 100000u;
    while ((FLEXCAN1_MCR & (1u << 25)) && --to)
    {
    }

    // freeze mode
    FLEXCAN1_MCR |= (1u << 30) | (1u << 28);
    to = 100000u;

    while (!(FLEXCAN1_MCR & (1u << 24)) && --to)
    {
    } /* FRZ_ACK=1 */
    if (to == 0u)
        return 1;

    /**
     * 비트타이밍 + loopback(freeze에서만 쓰기 가능)
     * PRESDIV=63, RJW=2, PSEG1=3, PSEG2=3, LPB=1, PROPSEG=2 (loopback 이라 baud 는 자유)
     */
    FLEXCAN1_CTRL1 = (63u << 24) | (2u << 22) | (3u << 19) | (3u << 16) | (1u << 12) | (2u << 0);

    /* Message Buffer 초기화 : MAXMB=15, 16개의 MB 모두 INACTIVE(CODE=0) */
    FLEXCAN1_MCR = (FLEXCAN1_MCR & ~0x7Fu) | 15u;
    for (uint32_t i = 0; i < 16; i++)
    {
        volatile uint32_t *mb = FLEXCAN1_MB(i);
        mb[0] = 0u;
        mb[1] = 0u;
        mb[2] = 0u;
        mb[3] = 0u;
    }

    /* Freeze 해제 -> 동기화 완료(NOT_RDY=0) 대기 */
    FLEXCAN1_MCR &= ~((1u << 30) | (1u << 28));
    to = 1000000u;
    while ((FLEXCAN1_MCR & (1u << 27)) && --to)
    {
    } /* NOT_RDY=0 */
    return (to == 0u) ? 1 : 0;
}