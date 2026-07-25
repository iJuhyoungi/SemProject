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

static void print_sr(const char *label)
{
    uint8_t sr;
    
    if (FlexSPI_ReadStatus(&sr) != FLS_IP_OK)
    {
        UART1_SendString(label);
        UART1_SendString(" status read FAILED\r\n");
        return;
    }   

    UART1_SendString(label);
    UART1_SendString(" SR=0x");
    uart_hex8(sr);
    UART1_SendString(" WIP=");
    UART1_SendChar((sr & FLS_STATUS_WIP) ? '1' : '0');
    UART1_SendString(" WEL=");
    UART1_SendChar((sr & FLS_STATUS_WEL) ? '1' : '0');
    UART1_SendString("\r\n");
}

static void report_wel_latch(void)
{
    UART1_SendString("[FLS] --- WEL latch test (flash 내용 변경 없음) ---\r\n");

    print_sr("[FLS]   boot   :");

    FlexSPI_WriteDisable();
    print_sr("[FLS]   WRDI   :");   /* 기대: WEL=0 */
    
    FlexSPI_WriteEnable();
    print_sr("[FLS]   WREN   :");   /* 기대: WEL=1 */
    
    FlexSPI_WriteDisable();
    print_sr("[FLS]   relock :");   /* 기대: WEL=0 — 실험 끝나면 다시 잠근다 */
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

/* 8바이트를 라벨과 함께 16진수로 찍는다. */
static void dump8(const char *label, const uint8_t *d)
{
    uint32_t i;

    UART1_SendString(label);
    for (i = 0u; i < 8u; i++)
    {
        UART1_SendChar(' ');
        uart_hex8(d[i]);
    }
    UART1_SendString("\r\n");
}

static void report_erase(void)
{
    uint8_t        before[8];
    uint8_t        after[8];
    Fls_EraseTrace trace;
    Fls_IpStatus   st;
    uint32_t       i;
    uint32_t       blank = 1u;

    UART1_SendString("[FLS] --- sector erase @0x7FF000 ---\r\n");

    /* ① 안전장치부터 시험한다. 이미지 영역(0x000000)은 거부되어야 한다. */
    st = Fls_EraseSector(0x00000000u, &trace);
    UART1_SendString("[FLS]   guard 0x000000 : ");
    UART1_SendString((st == FLS_IP_E_FORBIDDEN) ? "REJECTED (정상)\r\n"
                                                : "!!! 통과됨 - 중단 !!!\r\n");
    if (st != FLS_IP_E_FORBIDDEN)
    {
        return;   /* 가드가 안 먹으면 진짜 erase 는 시도조차 하지 않는다 */
    }

    /* ② erase 전 내용 */
    if (FlexSPI_ReadData(FLS_TEST_SECTOR, before, 8u) == FLS_IP_OK)
    {
        dump8("[FLS]   before :", before);
    }

    st = Fls_EraseSector(FLS_TEST_SECTOR, &trace);
    if (st != FLS_IP_OK)
    {
        UART1_SendString("[FLS]   erase FAILED, status=");
        UART1_SendHex32((uint32_t)st);
        UART1_SendString("\r\n");
        return;
    }
    
    /* erase 가 진짜 실행됐다는 증거: 명령 직후 WIP=1, 그리고 폴링 횟수 */
    UART1_SendString("[FLS]   SR after cmd = 0x");
    uart_hex8(trace.sr_after_cmd);
    UART1_SendString(" (WIP=");
    UART1_SendChar((trace.sr_after_cmd & FLS_STATUS_WIP) ? '1' : '0');
    UART1_SendString(")  poll count = ");
    UART1_SendHex32(trace.poll_count);
    UART1_SendString("\r\n");

    /* ④ erase 후 내용 — 전부 0xFF 여야 한다 */
    if (FlexSPI_ReadData(FLS_TEST_SECTOR, after, 8u) == FLS_IP_OK)
    {
        dump8("[FLS]   after  :", after);

        for (i = 0u; i < 8u; i++)
        {
            if (after[i] != 0xFFu)
            {
                blank = 0u;
            }
        }   
        UART1_SendString(blank ? "[FLS]   ERASE OK (all 0xFF)\r\n"
                                : "[FLS]   ERASE INCOMPLETE\r\n");
    }                          
}

static void report_program(void)
{
    /* 패턴은 스택(DTCM)에 둔다 — program_core 가 ITCM 에서 이걸 읽으므로 RAM 이어야 한다. */
    uint8_t        pattern[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0x12, 0x34 };
    uint8_t        readback[8];
    Fls_EraseTrace trace;
    Fls_IpStatus   st;
    uint32_t       i;
    uint32_t       match = 1u;

    UART1_SendString("[FLS] --- page program @0x7FF000 ---\r\n");

    /* report_erase 가 방금 이 섹터를 0xFF 로 지웠다. 그 위에 패턴을 쓴다. */
    st = Fls_ProgramPage(FLS_TEST_SECTOR, pattern, 8u, &trace);

    UART1_SendString("[FLS]   SR after cmd = 0x");
    uart_hex8(trace.sr_after_cmd);
    UART1_SendString(" (WIP=");
    UART1_SendChar((trace.sr_after_cmd & FLS_STATUS_WIP) ? '1' : '0');
    UART1_SendString(")  poll count = ");
    UART1_SendHex32(trace.poll_count);
    UART1_SendString("\r\n");

    if (st != FLS_IP_OK)
    {
        UART1_SendString("[FLS]   program FAILED\r\n");
        return;
    }

    dump8("[FLS]   wrote  :", pattern);

    if (FlexSPI_ReadData(FLS_TEST_SECTOR, readback, 8u) == FLS_IP_OK)
    {
        dump8("[FLS]   read   :", readback);
        for (i = 0u; i < 8u; i++)
        {
            if (readback[i] != pattern[i])
            {
                match = 0u;
            }
        }
        UART1_SendString(match ? "[FLS]   PROGRAM OK\r\n"
                                : "[FLS]   PROGRAM MISMATCH\r\n");
    }
}

/* NOR flash 의 정체 실증: program 은 비트를 1->0 으로만 바꾼다.
* 지우지 않고 덮어쓰면 원하는 값이 아니라 (기존 AND 신규) 가 나온다. */
static void report_ebw_demo(void)
{
    uint8_t        hi[8] = { 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0 };
    uint8_t        lo[8] = { 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F };
    uint8_t        rb[8];
    Fls_EraseTrace trace;
    uint32_t       addr = FLS_TEST_SECTOR + 16u;   /* 같은 섹터의 다른 자리 (이미 0xFF) */

    UART1_SendString("[FLS] --- erase-before-write demo @+16 ---\r\n");

    /* 0xFF 위에 0xF0 을 쓴다 → 0xFF AND 0xF0 = 0xF0 */
    Fls_ProgramPage(addr, hi, 8u, &trace);
    if (FlexSPI_ReadData(addr, rb, 8u) == FLS_IP_OK)
    {
        dump8("[FLS]   after 0xF0 :", rb);
    }

    /* 지우지 않고 0x0F 를 덮어쓴다 → 0xF0 AND 0x0F = 0x00 (0x0F 가 아니다!) */
    Fls_ProgramPage(addr, lo, 8u, &trace);
    if (FlexSPI_ReadData(addr, rb, 8u) == FLS_IP_OK)
    {
        dump8("[FLS]   after 0x0F :", rb);
    }

    UART1_SendString("[FLS]   -> 0x0F 를 썼는데 0x00. 비트는 1->0 만, 되돌리려면 erase 뿐.\r\n");
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

    report_wel_latch();

    report_erase();         // 실험한 섹터를 실제로 erase하고 WIP 폴링 구간은 ITCM에서 동작
    report_program();
    report_ebw_demo();

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
