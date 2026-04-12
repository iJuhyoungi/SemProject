#include <stdint.h>
#include "uart.h"
#include "clock.h"
#include "led.h"

/* 🚀 메인 앱(App Slot A)이 구워질 시작 주소 */
#define APP_SLOTA_ADDR 0x60040000

/* Cortex-M7 Vector Table Offset Register 주소 */
#define SCB_VTOR (*(volatile uint32_t *)0xE000ED08)

/* 메인 앱으로 영원히 떠나는(돌아오지 않는) 점프 함수 */
void Jump_To_App(uint32_t app_address)
{
    /* 1. 앱의 벡터 테이블 첫 2개의 값을 읽어옵니다.
     * 첫 번째 값(Offset 0) = 앱이 사용할 스택 포인터(MSP)
     * 두 번째 값(Offset 4) = 앱의 첫 실행 함수(Reset_Handler) 주소 (PC) */
    uint32_t app_msp = *(volatile uint32_t *)app_address;
    uint32_t app_pc  = *(volatile uint32_t *)(app_address + 4);

    /* 2. 앱이 비어있는지(플래시가 지워진 상태인 0xFFFFFFFF) 방어 코드 */
    if (app_msp == 0xFFFFFFFF) {
        UART1_SendString("[BOOT] 🔴 Error: No Application Found at Slot A!\r\n");
        return;
    }

    UART1_SendString("[BOOT] 🚀 Jumping to Application...\r\n\r\n");

    /* 3. 부트로더가 켜놓은 모든 인터럽트를 끕니다. 
     * (앱이 깨끗한 상태에서 시작할 수 있도록) */
    __asm volatile ("cpsid i");

    /* 4. VTOR(인터럽트 주소록)을 앱의 시작 주소로 교체 */
    SCB_VTOR = app_address;

    /* 5. 파이프라인 버퍼 비우기 (안전빵 배리어) */
    __asm volatile ("dsb");
    __asm volatile ("isb");

    /* 🚀 6. 대망의 점프! 스택 포인터를 갈아끼우고 PC를 날립니다. */
    __asm volatile (
        "msr msp, %0 \n"    /* Main Stack Pointer 교체 */
        "bx %1 \n"          /* Branch and Exchange (돌아오지 않는 점프) */
        :: "r" (app_msp), "r" (app_pc)
    );
}

int main(void)
{
    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" 🛡️ OTA Bootloader Started! \r\n");
    UART1_SendString("=================================\r\n");

    /* 부트로더 화면을 잠시 감상할 수 있게 딜레이 */
    for(volatile int i=0; i<5000000; i++) {
        __asm("nop");
    }

    UART1_SendString("[BOOT] Checking App Slot A (0x60040000)...\r\n");
    
    /* 점프 함수 호출 */
    Jump_To_App(APP_SLOTA_ADDR);

    /* 만약 점프에 실패했거나(빈 플래시), 앱에서 리턴되어 버린 경우 */
    UART1_SendString("[BOOT] 💀 System Halted.\r\n");
    while (1) {
        __asm("wfi");
    }
    
    return 0;
}