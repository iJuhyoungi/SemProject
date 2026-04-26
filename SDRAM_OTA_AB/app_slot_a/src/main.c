#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "bootctrl_runtime.h"
// #include "boot_policy.h"
// #include "bootctrl_flash.h"

extern volatile uint32_t g_bootctrl_dbg;
extern volatile uint32_t g_bootctrl_dbg_status;
extern volatile uint32_t g_bootctrl_dbg_sts0_before;
extern volatile uint32_t g_bootctrl_dbg_arbidle_wait;

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm("nop");
    }
}

static void App_PrintBootCtrl(const boot_ctrl_t *ctrl)
{
    UART1_SendString("[APP-A-SDRAM] bootctrl.magic          = 0x");
    UART1_SendHex32(ctrl->magic);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-A-SDRAM] bootctrl.active_slot    = 0x");
    UART1_SendHex32(ctrl->active_slot);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-A-SDRAM] bootctrl.pending_slot   = 0x");
    UART1_SendHex32(ctrl->pending_slot);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-A-SDRAM] bootctrl.boot_success   = 0x");
    UART1_SendHex32(ctrl->boot_success);
    UART1_SendString("\r\n");

    UART1_SendString("[APP-A-SDRAM] bootctrl.boot_attempts  = 0x");
    UART1_SendHex32(ctrl->boot_attempts);
    UART1_SendString("\r\n");
}

int main(void)
{
    boot_ctrl_t ctrl;

    LED_Init();

    UART1_SendString("\r\n[APP-A-SDRAM] main entered\r\n");

    if (BootCtrl_Runtime_Read(&ctrl)) {
        UART1_SendString("[APP-A-SDRAM] bootctrl read OK\r\n");
        App_PrintBootCtrl(&ctrl);
    } else {
        UART1_SendString("[APP-A-SDRAM] bootctrl read FAILED\r\n");
    }

    UART1_SendString("[APP-A-SDRAM] Phase B precheck start\r\n");

    if (BootCtrl_BootloaderWrite_Precheck()) {
        UART1_SendString("[APP-A-SDRAM] Phase B precheck OK\r\n");
    } else {
        UART1_SendString("[APP-A-SDRAM] Phase B precheck FAILED\r\n");
    }

    UART1_SendString("[APP-A-SDRAM] Phase B write attempt start\r\n");

    if (BootCtrl_RuntimeConfirm_SlotA()) {
        UART1_SendString("[APP-A-SDRAM] Phase B confirm write SUCCESS (unexpected at this stage)\r\n");
    } else {
        UART1_SendString("[APP-A-SDRAM] Phase B confirm write FAILED (expected before real flash driver)\r\n");

        UART1_SendString("[APP-A-SDRAM] dbg = 0x");
        UART1_SendHex32(g_bootctrl_dbg);
        UART1_SendString("\r\n");

        UART1_SendString("[APP-A-SDRAM] dbg_status = 0x");
        UART1_SendHex32(g_bootctrl_dbg_status);
        UART1_SendString("\r\n");

        UART1_SendString("[APP-A-SDRAM] dbg_sts0_before = 0x");
        UART1_SendHex32(g_bootctrl_dbg_sts0_before);
        UART1_SendString("\r\n");

        UART1_SendString("[APP-A-SDRAM] dbg_arbidle_wait = 0x");
        UART1_SendHex32(g_bootctrl_dbg_arbidle_wait);
        UART1_SendString("\r\n");
    }

    UART1_SendString("[APP-A-SDRAM] relocation done, heartbeat start\r\n");

    while (1) {
        LED_On();
        UART1_SendString("[APP-A-SDRAM] heartbeat\r\n");
        delay_loop(30000000);

        LED_Off();
        delay_loop(30000000);
    }
}
