#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "flexspi_ip.h"

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

static void uart_hex8(uint8_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    UART1_SendChar(digits[(v >> 4) & 0x0Fu]);
    UART1_SendChar(digits[v & 0x0Fu]);
}

/* JEDEC manufacturer ID 는 제조사마다 고정된 한 바이트다. */
static const char *jedec_vendor(uint8_t manufacturer)
{
    switch (manufacturer)
    {
    case 0xEF: return "Winbond";
    case 0x9D: return "ISSI";
    case 0xC2: return "Macronix";
    case 0x20: return "Micron";
    default:   return "unknown";
    }
}

static void report_jedec_id(void)
{
    uint8_t id[3];
    Fls_IpStatus st;

    FlexSPI_InstallLut();

    st = FlexSPI_ReadJedecId(id);
    if (st != FLS_IP_OK)
    {
        UART1_SendString("[FLS] JEDEC ID read FAILED, status=");
        UART1_SendHex32((uint32_t)st);
        UART1_SendString("  (1=timeout, 2=cmd error)\r\n");
        return;
    }

    UART1_SendString("[FLS] JEDEC ID = ");
    uart_hex8(id[0]);
    UART1_SendChar(' ');
    uart_hex8(id[1]);
    UART1_SendChar(' ');
    uart_hex8(id[2]);
    UART1_SendString("  vendor=");
    UART1_SendString(jedec_vendor(id[0]));

    /* capacity 바이트는 용량의 지수다. 0x17 이면 2^23 = 8MB. */
    if (id[2] < 32u)
    {
        UART1_SendString("  size=");
        UART1_SendHex32(1u << id[2]);
        UART1_SendString(" bytes");
    }
    UART1_SendString("\r\n");
}

static void report_status(void)
{
    uint8_t sr;

    if (FlexSPI_ReadStatus(&sr) != FLS_IP_OK)
    {
        UART1_SendString("[FLS] Status read FAILED\r\n");
        return;
    }

    UART1_SendString("[FLS] Status-1 = 0x");
    uart_hex8(sr);
    UART1_SendString("  WIP=");
    UART1_SendChar((sr & FLS_STATUS_WIP) ? '1' : '0');
    UART1_SendString(" WEL=");
    UART1_SendChar((sr & FLS_STATUS_WEL) ? '1' : '0');
    UART1_SendString("\r\n");
}

static void verify_read(void)
{
    uint8_t      d[4];
    Fls_IpStatus st;
    uint32_t     ip_val;
    uint32_t     ahb_val;

    st = FlexSPI_ReadData(0x000000u, d, 4u);
    if (st != FLS_IP_OK)
    {
        UART1_SendString("[FLS] IP read FAILED, status=");
        UART1_SendHex32((uint32_t)st);
        UART1_SendString("\r\n");
        return;
    }

    ip_val  = (uint32_t)d[0] | ((uint32_t)d[1] << 8)
            | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
    ahb_val = *(volatile uint32_t *)0x60000000u;

    UART1_SendString("[FLS] IP read  @0x000000 = ");
    UART1_SendHex32(ip_val);
    UART1_SendString("\r\n[FLS] AHB read @0x60000000 = ");
    UART1_SendHex32(ahb_val);
    UART1_SendString(ip_val == ahb_val ? "\r\n[FLS] READ MATCH\r\n"
                                        : "\r\n[FLS] READ MISMATCH\r\n");
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[FLS] RT1020 Flash Driver — hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    report_jedec_id();
    report_status();
    verify_read();

    uint32_t beat = 0;
    while (1)
    {
        LED_On();
        delay_busy(20000000);
        LED_Off();
        delay_busy(20000000);

        UART1_SendString("[FLS] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}
