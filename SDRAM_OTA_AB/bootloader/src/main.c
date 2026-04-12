#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "boot_policy.h"
// #include "bootctrl_flash.h"

#define SCB_VTOR   (*(volatile uint32_t *)0xE000ED08)
#define NVIC_ICER0 (*(volatile uint32_t *)0xE000E180)
#define NVIC_ICPR0 (*(volatile uint32_t *)0xE000E280)
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010)

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

static void Jump_To_App(uint32_t app_address)
{
    uint32_t app_msp = *(volatile uint32_t *)app_address;
    uint32_t app_pc  = *(volatile uint32_t *)(app_address + 4);

    __asm volatile ("cpsid i");

    SYST_CSR = 0;
    NVIC_ICER0 = 0xFFFFFFFFu;
    NVIC_ICPR0 = 0xFFFFFFFFu;

    __asm volatile ("dsb");
    __asm volatile ("isb");

    SCB_VTOR = app_address;

    __asm volatile ("dsb");
    __asm volatile ("isb");

    __asm volatile (
        "msr msp, %0 \n"
        "bx %1 \n"
        :: "r"(app_msp), "r"(app_pc)
    );
}

int main(void)
{
    UART1_Init();
    LED_Init();

    UART1_SendString("\r\n=================================\r\n");
    UART1_SendString(" OTA Bootloader C-1 Started\r\n");
    UART1_SendString("=================================\r\n");

    delay_loop(3000000);

    const boot_ctrl_t *ctrl = Boot_GetCtrl();
    uint32_t slot = Boot_SelectSlot(ctrl);
    uint32_t base = Boot_GetSlotBase(slot);

    // /* 추가 */
    // if (Boot_IsPendingTrial(ctrl) && ctrl->pending_slot == slot) {
    //     BootCtrl_MarkTrialAttempt(slot);
    // }

    if (Boot_IsPendingTrial(ctrl)) {
        UART1_SendString("[BOOT] Pending trial detected\r\n");
        UART1_SendString("[BOOT] pending_slot = ");
        UART1_SendString((ctrl->pending_slot == SLOT_B) ? "B\r\n" : "A\r\n");
        UART1_SendString("[BOOT] boot_success = ");
        UART1_SendHex32(ctrl->boot_success);
        UART1_SendString("\r\n");
        UART1_SendString("[BOOT] boot_attempts = ");
        UART1_SendHex32(ctrl->boot_attempts);
        UART1_SendString("\r\n");
    }

    UART1_SendString("[BOOT] Selected slot: ");
    UART1_SendString((slot == SLOT_B) ? "B\r\n" : "A\r\n");

    UART1_SendString("[BOOT] Selected base: ");
    UART1_SendHex32(base);
    UART1_SendString("\r\n");

    if (!Boot_IsAppValid(base)) {
        UART1_SendString("[BOOT] Selected slot invalid, trying fallback...\r\n");

        slot = (slot == SLOT_B) ? SLOT_A : SLOT_B;
        base = Boot_GetSlotBase(slot);

        if (!Boot_IsAppValid(base)) {
            UART1_SendString("[BOOT] No valid app found in either slot.\r\n");
            while (1) {
                __asm("wfi");
            }
        }
    }

    UART1_SendString("[BOOT] Jumping to app...\r\n");
    Jump_To_App(base);

    UART1_SendString("[BOOT] Unexpected return from app.\r\n");
    while (1) {
        __asm("wfi");
    }
}
