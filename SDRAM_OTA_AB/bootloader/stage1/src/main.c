#include <stdint.h>
#include "uart.h"
#include "rt1020_regs.h"

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


 #define STAGE2_BASE 0x60004000u

 static void Jump_To_Stage2(uint32_t addr) {
    uint32_t app_msp=*(volatile uint32_t *)addr;
    uint32_t app_pc=*(volatile uint32_t*)(addr+4);

    __asm volatile ("cpsid i"); /* 인터럽트 비활성화 */
    __asm volatile ("dsb");     /* 모든 메모리 접근 완료 대기 */
    __asm volatile ("isb");     /* 명령어 파이프라인 초기화 */

    SCB_VTOR = addr;            /* Vector Table Offset Register에 Stage 2 벡터 테이블 주소 설정 */

    __asm volatile ("dsb");     /* VTOR 설정 완료 대기 */
    __asm volatile ("isb");     /* 명령어 파이프라인 초기화 */

    __asm volatile(
        "msr msp, %0   \n" /* MSP 레지스터에 Stage 2의 초기 SP 값 설정 */
        "bx %1         \n" /* Stage 2의 Reset_Handler로 점프 */
        :: "r"(app_msp), "r"(app_pc)
    );
 }

 static int Stage2_VectorSane(uint32_t addr) {
    uint32_t sp = *(volatile uint32_t *)addr;
    uint32_t pc = *(volatile uint32_t *)(addr + 4);

    /*SP가 DTCM/OCRAM 영역(0x2xxxxxxx) 안에 있는지*/
    if ((sp&0xF0000000u)!=0x20000000u){
        return 0;
    }
    /*PC가 FlexSPI FLASH (0x6xxxxxxx) 안에 있는지*/
    if((pc&0xF0000000u)!=0x60000000u){
        return 0;
    }

    if((pc&0x1u)!=0x1u){
        return 0;
    }

    return 1;
 }

 int main(void) {
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[BL1] Stage 1 immutable\r\n");
    UART1_SendString("\r\n=============================\r\n");


    if(!Stage2_VectorSane(STAGE2_BASE)) {
        UART1_SendString("[BL1] Stage 2 vector invalid! System Halted.\r\n");
        while(1){
            __asm("wfi");
        }
    }

    UART1_SendString("[BL1] Stage 2 vector sanity check OK\r\n");
    UART1_SendString("[BL1] Jumping to Stage 2(0x60004000))\r\n");

    Jump_To_Stage2(STAGE2_BASE);

    /*unreachable*/
    for(;;){
        __asm("wfi");
    }
 }