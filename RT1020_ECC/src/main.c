#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "Ecc.h"
#include "Ecc72.h"
#include "flexspi_ip.h"                 /* Fls_EraseSector/Fls_ProgramPage/FlexSPI_ReadData, FLS_TEST_SECTOR */

/* busy-wait */
static void delay_busy(volatile uint32_t n)
{
    while (n--)
    {
        __asm volatile("nop");
    }
}

/* E-2: (13,8) SECDED 를 RAM 안에서 전수 검증한다. 모든 데이터(0~255)에 대해
 * ① 무오류 → NO_ERROR + 값 일치, ② 단일비트 13개 전수 → CORRECTED + 값 복구,
 * ③ 이중비트(인접 쌍) → UNCORRECTABLE 검출. 하드웨어(flash) 없이 수학만 격리 검증. */
static void report_ecc_selftest(void)
{
    uint32_t okClean=0u;
    uint32_t okCorr=0u;
    uint32_t okDet=0u;
    uint32_t fail=0u;
    uint16_t d;

    UART1_SendString("[ECC] === E-2 (13,8) SECDED self-test ===\r\n");

    for(d=0u;d<256u;++d){
        uint8_t data=(uint8_t)d;
        uint16_t cw=Ecc_Encode(data);
        uint8_t out;
        uint8_t b;

        if((Ecc_Decode(cw,&out)==ECC_NO_ERROR)&&(out==data)){
            okClean++;
        }
        else{
            fail++;
        }

        for(b=0u;b<13u;++b){
            uint16_t bad=(uint16_t)(cw^(1u<<b));
            if((Ecc_Decode(bad,&out)==ECC_CORRECTED)&&(out==data)){
                okCorr++;
            }else{
                fail++;
            }
        }

        for(b=0u;b<12u;++b){
            uint16_t bad=(uint16_t)(cw^(1u<<b)^(1u<<(b+1u)));
            uint8_t o2;
            if(Ecc_Decode(bad,&o2)==ECC_UNCORRECTABLE){
                okDet++;
            }else{
                fail++;
            }
        }
    }

    UART1_SendString("[ECC]   clean   OK = ");  UART1_SendHex32(okClean);  /* 기대 0x100  = 256  */
    UART1_SendString("\r\n[ECC]   correct OK = "); UART1_SendHex32(okCorr);  /* 기대 0xD00  = 3328 */
    UART1_SendString("\r\n[ECC]   detect  OK = "); UART1_SendHex32(okDet);   /* 기대 0xC00  = 3072 */
    UART1_SendString("\r\n[ECC]   fail       = "); UART1_SendHex32(fail);    /* 기대 0        */
    UART1_SendString(fail == 0u ? "\r\n[ECC]   ALL PASS\r\n" : "\r\n[ECC]   FAIL\r\n");
}

static void report_ecc72_selftest(void)
{
    uint32_t okClean = 0u;
    uint32_t okCorr  = 0u;
    uint32_t okDet   = 0u;
    uint32_t fail    = 0u;
    uint64_t seed    = 0x0123456789ABCDEFULL;
    uint32_t t;
    const uint32_t NTEST = 16u;

    Ecc72_Init();
    UART1_SendString("[ECC] === E-3 (72,64) Hsiao SECDED self-test ===\r\n");

    for (t = 0u; t < NTEST; t++)
    {
        uint64_t D;
        uint8_t  ecc;
        uint8_t  i, j;

        /* 앞 4개는 고정 엣지 패턴, 나머지는 xorshift64 로 생성 (곱셈 없이 shift/XOR 만). */
        if      (t == 0u) { D = 0ULL; }
        else if (t == 1u) { D = ~0ULL; }
        else if (t == 2u) { D = 0xAAAAAAAAAAAAAAAAULL; }
        else if (t == 3u) { D = 0x5555555555555555ULL; }
        else { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; D = seed; }

        ecc = Ecc72_Encode(D);

        /* Error 없음 */
        {
            uint64_t d = D;
            if ((Ecc72_Decode(&d, ecc) == ECC_NO_ERROR) && (d == D)) { okClean++; }
            else { fail++; }
        }

        /* 단일비트 : 데이터 64 + ECC 8 = 72 위치 */
        for (i = 0u; i < 64u; i++)
        {
            uint64_t d = D ^ ((uint64_t)1u << i);
            if ((Ecc72_Decode(&d, ecc) == ECC_CORRECTED) && (d == D)) { okCorr++; }
            else { fail++; }
        }
        for (i = 0u; i < 8u; i++)
        {
            uint64_t d = D;
            uint8_t  e = (uint8_t)(ecc ^ (1u << i));
            if ((Ecc72_Decode(&d, e) == ECC_CORRECTED) && (d == D)) { okCorr++; }
            else { fail++; }
        }

        /* 이중비트 : 72위치 중 2개. 전부 UNCORRECTABLE 이어야 한다. */
        for (i = 0u; i < 72u; i++)
        {
            for (j = (uint8_t)(i + 1u); j < 72u; j++)
            {
                uint64_t d = D;
                uint8_t  e = ecc;
                if (i < 64u) { d ^= ((uint64_t)1u << i); } else { e ^= (uint8_t)(1u << (i - 64u)); }
                if (j < 64u) { d ^= ((uint64_t)1u << j); } else { e ^= (uint8_t)(1u << (j - 64u)); }
                if (Ecc72_Decode(&d, e) == ECC_UNCORRECTABLE) { okDet++; }
                else { fail++; }
            }
        }
    }

    UART1_SendString("[ECC]   clean   OK = ");    UART1_SendHex32(okClean); /* 기대 0x10    = 16    */
    UART1_SendString("\r\n[ECC]   correct OK = "); UART1_SendHex32(okCorr);  /* 기대 0x480   = 1152  */
    UART1_SendString("\r\n[ECC]   detect  OK = "); UART1_SendHex32(okDet);   /* 기대 0x9FC0  = 40896 */
    UART1_SendString("\r\n[ECC]   fail       = "); UART1_SendHex32(fail);     /* 기대 0             */
    UART1_SendString(fail == 0u ? "\r\n[ECC]   ALL PASS\r\n" : "\r\n[ECC]   FAIL\r\n");
}

/* 바이트 하나를 16진수로. */
static void uart_hex8(uint8_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    UART1_SendChar(digits[(v >> 4) & 0x0Fu]);
    UART1_SendChar(digits[v & 0x0Fu]);
}

/* n 바이트를 라벨과 함께 덤프. */
static void dumpN(const char *label, const uint8_t *d, uint32_t n)
{
    uint32_t i;
    UART1_SendString(label);
    for (i = 0u; i < n; i++) { UART1_SendChar(' '); uart_hex8(d[i]); }
    UART1_SendString("\r\n");
}

/* 64비트 데이터를 8바이트(리틀엔디언)로 풀고 다시 모은다. */
static void put_u64(uint8_t *p, uint64_t v)
{
    uint8_t i;
    for (i = 0u; i < 8u; i++) { p[i] = (uint8_t)(v >> (8u * i)); }
}
static uint64_t get_u64(const uint8_t *p)
{
    uint64_t v = 0u;
    uint8_t  i;
    for (i = 0u; i < 8u; i++) { v |= ((uint64_t)p[i]) << (8u * i); }
    return v;
}

static void report_ecc_flash(void)
{
    const uint32_t base = FLS_TEST_SECTOR;                 /* 0x7FF000: Fls 허용영역 + 이미지 밖 */
    uint64_t       D    = 0xA5A5A5A5A5A5A5A5ULL;           /* byte0=0xA5=1010_0101, 켜진 비트가 있어 1->0 주입 가능 */
    Fls_EraseTrace trace;
    uint8_t        cw[9];
    uint8_t        rb[9];
    uint8_t        ecc;
    uint8_t        mask;
    uint64_t       Dr;
    Ecc_Status     st;

    UART1_SendString("[ECC] === E-4 flash-backed ECC (real 1->0 bit-flip inject) ===\r\n");

    Ecc72_Init();
    FlexSPI_InstallLut();   /* IP 명령용 LUT 설치 — 데모 제거로 호출이 사라졌으므로 여기서 꼭 한다 */

    /* 섹터를 0xFF 로 지운 뒤, 데이터 8B + ECC 1B 저장 */
    (void)Fls_EraseSector(base, &trace);
    ecc = Ecc72_Encode(D);
    put_u64(cw, D);
    (void)Fls_ProgramPage(base, cw, 8u, &trace);         /* 데이터 8바이트          */
    (void)Fls_ProgramPage(base + 8u, &ecc, 1u, &trace);  /* ECC 1바이트 (base+8)     */

    /* Error 없음 */
    (void)FlexSPI_ReadData(base, rb, 9u);
    dumpN("[ECC]   stored bytes  :", rb, 9u);
    Dr = get_u64(rb);
    st = Ecc72_Decode(&Dr, rb[8]);
    UART1_SendString(((st == ECC_NO_ERROR) && (Dr == D))
                        ? "[ECC]   clean read    : NO_ERROR : OK\r\n"
                        : "[ECC]   clean read    : FAIL\r\n");

    /* 단일비트 : byte0 의 bit0(현재 1)을 flash 에서 1-> 0 으로 (0xFE 프로그램) */
    mask = (uint8_t)(0xFFu ^ (1u << 0));
    (void)Fls_ProgramPage(base, &mask, 1u, &trace);
    (void)FlexSPI_ReadData(base, rb, 9u);
    dumpN("[ECC]   after 1 inject:", rb, 9u);   /* byte0 이 A5 -> A4 로 바뀜 */
    Dr = get_u64(rb);
    st = Ecc72_Decode(&Dr, rb[8]);
    UART1_SendString(((st == ECC_CORRECTED) && (Dr == D))
                        ? "[ECC]   1-bit error   : CORRECTED, data restored : OK\r\n"
                        : "[ECC]   1-bit error   : FAIL\r\n");

    /* 두 번째 비트 : byte0 의 bit2(현재 1)도 1->0 (0xFB) -> 원본 대비 2비트 오류 */
    mask = (uint8_t)(0xFFu ^ (1u << 2));
    (void)Fls_ProgramPage(base, &mask, 1u, &trace);
    (void)FlexSPI_ReadData(base, rb, 9u);
    dumpN("[ECC]   after 2 inject:", rb, 9u);   /* byte0 이 A4 -> A0 */
    Dr = get_u64(rb);
    st = Ecc72_Decode(&Dr, rb[8]);
    UART1_SendString((st == ECC_UNCORRECTABLE)
                        ? "[ECC]   2-bit error   : UNCORRECTABLE, detected : OK\r\n"
                        : "[ECC]   2-bit error   : FAIL\r\n");
}

int main(void)
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[ECC] RT1020 ECC / RAS — hello from main()\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    report_ecc_selftest();
    report_ecc72_selftest();
    report_ecc_flash();

    uint32_t beat = 0;
    while (1)
    {
        LED_On();
        delay_busy(20000000);
        LED_Off();
        delay_busy(20000000);

        UART1_SendString("[ECC] beat ");
        UART1_SendHex32(beat++);
        UART1_SendString("\r\n");
    }

    return 0;
}
