#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "bootctrl_runtime_b.h"

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

static void AppB_PrintBootCtrl(const boot_ctrl_t *ctrl)
{
    UART1_SendString("[APP-B] bootctrl.magic          = 0x");
    UART1_SendHex32(ctrl->magic);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-B] bootctrl.active_slot    = 0x");
    UART1_SendHex32(ctrl->active_slot);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-B] bootctrl.pending_slot   = 0x");
    UART1_SendHex32(ctrl->pending_slot);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-B] bootctrl.boot_success   = 0x");
    UART1_SendHex32(ctrl->boot_success);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-B] bootctrl.boot_attempts  = 0x");
    UART1_SendHex32(ctrl->boot_attempts);
    UART1_SendString("\r\n");
}

int main(void)
{
    boot_ctrl_t ctrl;

    LED_Init();

    UART1_SendString("\r\n[APP-B] main entered\r\n");

    if (BootCtrl_RuntimeB_Read(&ctrl)) {
        UART1_SendString("[APP-B] bootctrl read OK\r\n");
        AppB_PrintBootCtrl(&ctrl);
    } else {
        UART1_SendString("[APP-B] bootctrl read FAILED\r\n");
    }

    /*
     * Slot B는 trial 상태일 때만 confirm write 수행.
     */
    if (ctrl.magic == BOOT_CTRL_MAGIC &&
        ctrl.pending_slot == SLOT_B &&
        ctrl.boot_success == BOOT_SUCCESS_NO) {

        UART1_SendString("[APP-B] Slot B trial detected\r\n");
        UART1_SendString("[APP-B] confirm write start\r\n");

        if (BootCtrl_RuntimeConfirm_SlotB()) {
            UART1_SendString("[APP-B] confirm write SUCCESS\r\n");
        } else {
            UART1_SendString("[APP-B] confirm write FAILED\r\n");
        }
    } else {
        UART1_SendString("[APP-B] no confirm needed\r\n");
    }

    UART1_SendString("[APP-B] heartbeat start\r\n");

    while (1) {
        LED_On();
        UART1_SendString("[APP-B] heartbeat\r\n");
        delay_loop(30000000);

        LED_Off();
        delay_loop(30000000);
    }
}