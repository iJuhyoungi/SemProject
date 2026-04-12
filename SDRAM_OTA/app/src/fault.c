#include <stdint.h>
#include "uart.h"
#include "mimxrt1020.h"

/* 직접 레지스터 주소 사용 */
#define SCB_CFSR_REG   (*(volatile uint32_t *)0xE000ED28)
#define SCB_HFSR_REG   (*(volatile uint32_t *)0xE000ED2C)
#define SCB_MMFAR_REG  (*(volatile uint32_t *)0xE000ED34)
#define SCB_BFAR_REG   (*(volatile uint32_t *)0xE000ED38)
#define SCB_ABFSR_REG  (*(volatile uint32_t *)0xE000EFA8)

/* 아주 단순한 HEX 출력기: UART1_SendChar만 사용 */
__attribute__((section(".boot_text"), noinline, used))
static void Fault_SendHex32_Raw(uint32_t val)
{
    for (int j = 0; j < 8; j++) {
        uint32_t nibble = (val >> ((7 - j) * 4)) & 0xF;
        char c = (nibble < 10) ? ('0' + nibble) : ('A' + (nibble - 10));
        UART1_SendChar(c);
    }
}

__attribute__((section(".boot_text"), noinline, used))
static void Fault_PrintReg(const char *name, uint32_t value)
{
    UART1_SendString(name);
    UART1_SendString("=0x");
    Fault_SendHex32_Raw(value);
    UART1_SendString("\r\n");
}

__attribute__((section(".boot_text"), noinline, used))
void HardFault_Handler(void)
{
    __asm volatile ("cpsid i");

    UART1_SendString("\r\n[FAULT] HardFault\r\n");

    /* 먼저 읽고, 그 다음 출력 */
    uint32_t cfsr  = SCB_CFSR_REG;
    uint32_t hfsr  = SCB_HFSR_REG;
    uint32_t mmfar = SCB_MMFAR_REG;
    uint32_t bfar  = SCB_BFAR_REG;

    Fault_PrintReg("ABFSR", SCB_ABFSR_REG);
    Fault_PrintReg("CFSR",  cfsr);
    Fault_PrintReg("HFSR",  hfsr);
    Fault_PrintReg("MMFAR", mmfar);
    Fault_PrintReg("BFAR",  bfar);

    while(1){}
    while (1) {
        __asm("nop");
    }
}

__attribute__((section(".boot_text"), noinline, used))
void BusFault_Handler(void)
{
    __asm volatile ("cpsid i");

    UART1_SendString("\r\n[FAULT] BusFault\r\n");

    uint32_t cfsr = SCB_CFSR_REG;
    uint32_t bfar = SCB_BFAR_REG;

    Fault_PrintReg("CFSR", cfsr);
    Fault_PrintReg("BFAR", bfar);

    while (1) {
        __asm("nop");
    }
}

__attribute__((section(".boot_text"), noinline, used))
void MemManage_Handler(void)
{
    __asm volatile ("cpsid i");

    UART1_SendString("\r\n[FAULT] MemManage\r\n");

    uint32_t cfsr  = SCB_CFSR_REG;
    uint32_t mmfar = SCB_MMFAR_REG;

    Fault_PrintReg("CFSR",  cfsr);
    Fault_PrintReg("MMFAR", mmfar);

    while (1) {
        __asm("nop");
    }
}
