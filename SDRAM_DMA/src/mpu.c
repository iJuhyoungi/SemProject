#include <stdint.h>
#include "mimxrt1020.h"
#include "rt1020_regs.h"

#define MPU_CTRL_ENABLE_Msk         (1UL << 0)
#define MPU_CTRL_PRIVDEFENA_Msk     (1UL << 2)

/* 🚀 핵심: 이 함수를 무조건 플래시(0x6000...)에 묶어둡니다! */
__attribute__((section(".boot_text"))) void MPU_Init(void)
{
    /* 1. 설정 중 예기치 않은 접근을 막기 위해 MPU 비활성화 */
    __asm("dsb");
    __asm("isb");
    MPU_CTRL = 0;

    /* -------------------------------------------------------------
     * [Region 0] Flash / XIP (0x60000000, 8MB)
     * 속성: Normal Memory, Cacheable, Execute Allow
     * ------------------------------------------------------------- */
    MPU_RNR = 0; // 0번 영역 선택
    MPU_RBAR = 0x60000000;
    MPU_RASR = (0 << 28) | /* XN = 0 (실행 가능) */
               (3 << 24) | /* AP = 3 (읽기/쓰기 모두 허용) */
               (0 << 19) | /* TEX = 0 */
               (0 << 18) | /* S = 0 (Shareable 아님) */
               (1 << 17) | /* C = 1 (Cacheable) */
               (1 << 16) | /* B = 1 (Bufferable) */
               (22 << 1) | /* Size = 8MB (2^23 -> 23-1=22) */
               (1 << 0);   /* Region Enable */

    /* -------------------------------------------------------------
     * [Region 1] SDRAM (0x80000000, 32MB)
     * 속성: Normal Memory, Cacheable (Write-Back, Write-Allocate), Execute Allow
     * ------------------------------------------------------------- */
    MPU_RNR = 1; // 1번 영역 선택
    MPU_RBAR = 0x80000000;
    MPU_RASR = (0 << 28) | /* XN = 0 (실행 가능) */
               (3 << 24) | /* AP = 3 (읽기/쓰기 모두 허용) */
               (1 << 19) | /* TEX = 1 (Write-Allocate 적용) */
               (0 << 18) | /* S = 0 (Shareable 아님) */
               (1 << 17) | /* C = 1 (Cacheable) */
               (1 << 16) | /* B = 1 (Bufferable) */
               (24 << 1) | /* Size = 32MB (2^25 -> 25-1=24) */
               (1 << 0);   /* Region Enable */

    /* -------------------------------------------------------------
     * [Region 2] Peripherals (0x40000000, 512MB)
     * 속성: Device Memory, Non-Cacheable, Execute Never (XN)
     * 안전장치: 주변장치 영역의 명령어를 실수로 실행하거나 캐시하지 못하게 묶음
     * ------------------------------------------------------------- */
    MPU_RNR = 2; // 2번 영역 선택
    MPU_RBAR = 0x40000000;
    MPU_RASR = (1 << 28) | /* XN = 1 (실행 절대 금지!) */
               (3 << 24) | /* AP = 3 (읽기/쓰기 허용) */
               (0 << 19) | /* TEX = 0 */
               (1 << 18) | /* S = 1 (Shareable) */
               (0 << 17) | /* C = 0 (Non-Cacheable) */
               (1 << 16) | /* B = 1 (Bufferable) */
               (28 << 1) | /* Size = 512MB (2^29 -> 29-1=28) */
               (1 << 0);   /* Region Enable */

    /* -------------------------------------------------------------
     * [Region 3] DMA Dedicated Buffer (0x81E00000, 2MB)
     * 속성: Normal Memory, Non-Cacheable, Shareable, Execute Allow
     * 목적: DMA와 CPU가 실시간으로 데이터를 공유해도 꼬이지 않는 절대 안전 구역
     * ------------------------------------------------------------- */
    MPU_RNR = 3; // 3번 영역 선택
    MPU_RBAR = 0x81E00000;
    MPU_RASR = (0 << 28) | /* XN = 0 (실행 가능) */
               (3 << 24) | /* AP = 3 (읽기/쓰기 모두 허용) */
               (1 << 19) | /* TEX = 1 (Normal Memory) */
               (1 << 18) | /* S = 1 (Shareable! DMA와 공유하므로 필수) */
               (0 << 17) | /* C = 0 (🚨 Non-Cacheable: 캐시 사용 금지!) */
               (0 << 16) | /* B = 0 (🚨 Non-Bufferable) */
               (20 << 1) | /* Size = 2MB (2^21 -> 21-1=20) */
               (1 << 0);   /* Region Enable */

    /* 2. MPU 활성화 및 백그라운드 메모리 맵(PRIVDEFENA) 사용 
     * (설정하지 않은 나머지 영역은 코어 기본 설정 따름) */
    MPU_CTRL = MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_ENABLE_Msk;
    __asm("dsb");
    __asm("isb");
}
