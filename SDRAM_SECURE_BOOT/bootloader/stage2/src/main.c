#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "bignum.h"
#include "verify.h"
#include "metadata.h"
#include "embedded_pubkey.h"

#define APP_A_BASE 0x60048000u
#define APP_B_BASE 0x60088000u

static void halt_on_fail(void)
{
    LED_On();
    __asm volatile("cpsid i");
    while (1)
    {
        __asm volatile("wfi");
    }
}

static void delay_loop(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; ++i)
    {
        __asm("nop");
    }
}

static void print_hex32(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[2] = {
        0,
    };
    UART1_SendString("0x");
    for (int i = 7; i >= 0; --i)
    {
        buf[0] = hex[(v >> (i * 4)) & 0xF];
        UART1_SendString(buf);
    }
}

// image header의 version 슬롯을 읽는 코드, erased flash면 0xFFFFFFFF
static uint32_t read_version(uint32_t base)
{
    return *(volatile uint32_t *)(base + IMG_VERSION_OFFSET);
}

// /*
//  * Rollback metadata sector 의 min_acceptable_version 읽기.
//  */
// static uint32_t read_min_version(void)
// {
//     uint32_t v = *(volatile uint32_t *)(METADATA_BASE + METADATA_MIN_VER_OFF);
//     return (v == 0xFFFFFFFFu) ? 0u : v;
// }

static const char *metadata_reason_str(metadata_reason_t r)
{
    switch (r)
    {
    case METADATA_OK:
        return "OK";
    case METADATA_BAD_MAGIC:
        return "BAD_MAGIC";
    case METADATA_BAD_SIGNATURE:
        return "BAD_SIGNATURE";
    default:
        return "UNKNOWN";
    }
}

int main(void)
{
    UART1_SendString("\r\n-----------------------------\r\n");
    UART1_SendString("[BL2] Stage 2 verified & running\r\n");
    UART1_SendString("-----------------------------\r\n");

    LED_Init();

    /*A/B version check*/
    uint32_t va = read_version(APP_A_BASE);
    uint32_t vb = read_version(APP_B_BASE);
    UART1_SendString("[BL2] App A version: ");
    print_hex32(va);
    UART1_SendString("\r\n");

    UART1_SendString("[BL2] App B version: ");
    print_hex32(vb);
    UART1_SendString("\r\n");

    metadata_t md;
    metadata_reason_t p_reason, b_reason;
    int ok = metadata_read_active(&md, &p_reason, &b_reason);

    UART1_SendString("[BL2] Metadata Primary: ");
    UART1_SendString(metadata_reason_str(p_reason));
    UART1_SendString("\r\n");
    UART1_SendString("[BL2] Metadata Backup:  ");
    UART1_SendString(metadata_reason_str(b_reason));
    UART1_SendString("\r\n");

    if (!ok)
    {
        UART1_SendString("[BL2] Metadata both invalid - halting (fail-safe)\r\n");
        halt_on_fail();
    }

    // if (!metadata_read_active(&md))
    // {
    //     UART1_SendString("[BL2] Metadata both invalid - halting (fail-safe)\r\n");
    //     halt_on_fail();
    // }

    uint32_t min_ver = md.min_acceptable_version;
    UART1_SendString("[BL2] Metadata seq = ");
    print_hex32(md.sequence_number);
    UART1_SendString("\r\n");
    UART1_SendString("[BL2] Min acceptable version = ");
    print_hex32(min_ver);
    UART1_SendString("\r\n");

    /*priority check*/
    uint32_t primary, secondary;
    uint32_t prim_ver, sec_ver;
    const char *prim_name, *sec_name;
    if (vb > va)
    {
        primary = APP_B_BASE;
        prim_name = "App B";
        prim_ver = vb;
        secondary = APP_A_BASE;
        sec_name = "App A";
        sec_ver = va;
    }
    else
    {
        primary = APP_A_BASE;
        prim_name = "App A";
        prim_ver = va;
        secondary = APP_B_BASE;
        sec_name = "App B";
        sec_ver = vb;
    }

    /* primary 시도 — version 검사 + verify */
    UART1_SendString("[BL2] Trying ");
    UART1_SendString(prim_name);
    UART1_SendString(" ...\r\n");
    if (prim_ver < min_ver)
    {
        UART1_SendString("[BL2] ");
        UART1_SendString(prim_name);
        UART1_SendString(" REJECTED - version below min (downgrade attempt)\r\n");
    }
    else if (verify_image(primary, EMBEDDED_PUBKEY_MODULUS))
    {
        UART1_SendString("[BL2] ");
        UART1_SendString(prim_name);
        UART1_SendString(" OK - jumping\r\n");
        jump_to_image(primary);
    }

    /* secondary fallback — 동일 검사 */
    UART1_SendString("[BL2] Falling back to ");
    UART1_SendString(sec_name);
    UART1_SendString(" ...\r\n");
    if (sec_ver < min_ver)
    {
        UART1_SendString("[BL2] ");
        UART1_SendString(sec_name);
        UART1_SendString(" REJECTED - version below min\r\n");
    }
    else if (verify_image(secondary, EMBEDDED_PUBKEY_MODULUS))
    {
        UART1_SendString("[BL2] ");
        UART1_SendString(sec_name);
        UART1_SendString(" OK - jumping\r\n");
        jump_to_image(secondary);
    }

    /* 둘 다 FAIL */
    UART1_SendString("[BL2] No valid image - halting\r\n");

    halt_on_fail();

    for (;;)
    {
        __asm("wfi");
    }
    return 0;
}
