#ifndef GLITCH_H
#define GLITCH_H

#include <stdint.h>

#if defined(GLITCH_SIM_BITFLIP) || defined(GLITCH_SIM_SKIP)

#include "uart.h"

#endif
/* 1비트만 손상됨 */
#ifdef GLITCH_SIM_BITFLIP

#ifndef GLITCH_BITFLIP_MASK
#define GLITCH_BITFLIP_MASK 0x00000001u
#endif

static inline uint32_t glitch_bitflip(uint32_t v)
{
    UART1_SendString("[Glitch] single-bit fault injected on verdict\r\n");
    return v ^ GLITCH_BITFLIP_MASK;
}

#else
static inline uint32_t glitch_bitflip(uint32_t v) 
{ 
    return v; 
}
#endif

#ifdef GLITCH_SIM_SKIP
#define GLITCH_SKIP_BRANCH_TAKEN() \
    (UART1_SendString("[Glitch] verdict branch skipped\r\n"), 1)
#else
#define GLITCH_SKIP_BRANCH_TAKEN() (0)
#endif

/* 실수로 남은 시뮬레이션 빌드를 부팅 로그에서 바로 알아채기 위한 배너입니다. */
#if defined(GLITCH_SIM_BITFLIP) || defined(GLITCH_SIM_SKIP)
#define GLITCH_BANNER() \
    UART1_SendString("[!!] GLITCH SIMULATION BUILD - NOT FOR RELEASE\r\n")
#else
#define GLITCH_BANNER() ((void)0)
#endif


#endif
