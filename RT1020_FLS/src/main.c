#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "flexspi_ip.h"
#include "Fls.h"
#include "Fee.h"

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


/* Std_ReturnType 을 사람이 읽을 수 있게 출력한다. */
static void print_ret(const char *label, Std_ReturnType r)
{
    UART1_SendString(label);
    UART1_SendString((r == E_OK) ? " E_OK\r\n" : " E_NOT_OK\r\n");
}

/* 요청한 job 이 끝날 때까지 Fls_MainFunction 을 반복 호출한다.
 * 실제 시스템에서는 스케줄러가 주기적으로 부르지만, 여기서는 루프로 흉내낸다. */
static MemIf_JobResultType fls_wait_job(void)
{
    while (Fls_GetJobResult() == MEMIF_JOB_PENDING)
    {
        Fls_MainFunction();
    }
    return Fls_GetJobResult();
}

static void report_fls_facade(void)
{
    uint8_t        wbuf[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    uint8_t        rbuf[8];
    Std_ReturnType r;
    uint32_t       i;
    uint32_t       match = 1u;

    UART1_SendString("[FLS] === F-5 MCAL Fls facade (비동기 job) ===\r\n");
    Fls_Init(&Fls_Config);

    /* 1. 비동기 erase: 요청은 즉시 반환하고, Fls_MainFunction 이 실제로 지운다. */
    r = Fls_Erase(0x007FF000u, 4096u);
    print_ret("[FLS]   Fls_Erase req :", r);
    UART1_SendString((fls_wait_job() == MEMIF_JOB_OK)
                         ? "[FLS]   erase result : MEMIF_JOB_OK\r\n"
                         : "[FLS]   erase result : FAILED\r\n");

    /* 2. 비동기 write. wbuf 는 스택(RAM)에 있으므로 program 이 안전하게 읽는다. */
    r = Fls_Write(0x007FF000u, wbuf, 8u);
    print_ret("[FLS]   Fls_Write req :", r);
    UART1_SendString((fls_wait_job() == MEMIF_JOB_OK)
                         ? "[FLS]   write result : MEMIF_JOB_OK\r\n"
                         : "[FLS]   write result : FAILED\r\n");

    /* 3. 비동기 read 후 방금 쓴 값과 비교한다. */
    r = Fls_Read(0x007FF000u, rbuf, 8u);
    print_ret("[FLS]   Fls_Read req  :", r);
    (void)fls_wait_job();
    for (i = 0u; i < 8u; i++)
    {
        if (rbuf[i] != wbuf[i])
        {
            match = 0u;
        }
    }
    UART1_SendString(match ? "[FLS]   read == write : OK\r\n"
                           : "[FLS]   read == write : MISMATCH\r\n");

    /* 4. DET 검증: 이미지 영역(허용 밖) 지우기 요청은 거부되어야 한다. */
    UART1_SendString("[FLS]   -- DET test: 이미지 영역(0x0) erase 요청 --\r\n");
    r = Fls_Erase(0x00000000u, 4096u);
    print_ret("[FLS]   Fls_Erase(0x0):", r); /* 기대: E_NOT_OK 와 [DET] 로그 */
}

/* Fee job 이 끝날 때까지 Fee_MainFunction 과 Fls_MainFunction 을 함께 돌린다.
 * 실제 시스템에서는 스케줄러가 두 MainFunction 을 주기적으로 부른다. */
static MemIf_JobResultType fee_wait_job(void)
{
    while (Fee_GetJobResult() == MEMIF_JOB_PENDING)
    {
        Fee_MainFunction();
        Fls_MainFunction();
    }
    return Fee_GetJobResult();
}

static void report_fee(void)
{
    uint8_t        a[8]  = { 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8 };
    uint8_t        b[8]  = { 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8 };
    uint8_t        c[16] = { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
                             0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF };
    uint8_t        r[16];
    Std_ReturnType s;
    uint32_t       i;
    uint32_t       ok;

    UART1_SendString("[FEE] === F-6b Fee write/read/persist ===\r\n");
    Fls_Init(&Fls_Config); /* Fee 는 Fls 를 아래 계층으로 쓰므로 먼저 초기화한다 */
    Fee_Init();

    /* 1) 지속성: 쓰기 전에 block1 을 읽어 지난 실행에서 남긴 값을 확인한다.
     *    재플래시 없이 리셋만 하면, 지난 부팅에서 쓴 값이 그대로 나와야 한다. */
    s = Fee_Read(1u, 0u, r, 8u);
    if (s == E_OK)
    {
        dump8("[FEE]   persisted blk1:", r);
    }
    else
    {
        UART1_SendString("[FEE]   persisted blk1: (없음, 첫 실행)\r\n");
    }

    /* 2) block1 = a 를 쓰고 읽어 왕복을 확인한다. */
    s = Fee_Write(1u, a);
    print_ret("[FEE]   Fee_Write(blk1,a):", s);
    (void)fee_wait_job();
    (void)Fee_Read(1u, 0u, r, 8u);
    ok = 1u;
    for (i = 0u; i < 8u; i++)
    {
        if (r[i] != a[i]) { ok = 0u; }
    }
    UART1_SendString(ok ? "[FEE]   write/read blk1 : OK\r\n"
                        : "[FEE]   write/read blk1 : MISMATCH\r\n");

    /* 3) latest-wins: 같은 block1 을 b 로 다시 쓰면, 읽었을 때 최신값 b 가 나와야 한다. */
    (void)Fee_Write(1u, b);
    (void)fee_wait_job();
    (void)Fee_Read(1u, 0u, r, 8u);
    ok = 1u;
    for (i = 0u; i < 8u; i++)
    {
        if (r[i] != b[i]) { ok = 0u; }
    }
    UART1_SendString(ok ? "[FEE]   latest-wins blk1: OK (b)\r\n"
                        : "[FEE]   latest-wins blk1: FAIL\r\n");

    /* 4) block2 = c (16B) 왕복. */
    (void)Fee_Write(2u, c);
    (void)fee_wait_job();
    (void)Fee_Read(2u, 0u, r, 16u);
    ok = 1u;
    for (i = 0u; i < 16u; i++)
    {
        if (r[i] != c[i]) { ok = 0u; }
    }
    UART1_SendString(ok ? "[FEE]   write/read blk2 : OK\r\n"
                        : "[FEE]   write/read blk2 : MISMATCH\r\n");
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

    /* F-5: MCAL Fls facade 를 비동기 job 모델로 시험한다. */
    report_fls_facade();

    /* F-6a: Fee 레이아웃 스캔 (빈 flash 에서 '블록 없음' 판정 확인). */
    report_fee();

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
