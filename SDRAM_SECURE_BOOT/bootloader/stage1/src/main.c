#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "rt1020_regs.h"
#include "crc32.h"
#include "sha256.h"
#include "bignum.h"

/**
 * Stage 1 immutable bootloader — Secure Boot.
 *
 * 책임:
 *  1. UART/LED init (UART 는 startup 의 IS_BOOTLOADER 분기가 담당)
 *  2. Stage 2 검증: vector sanity -> magic -> size -> CRC32
 *                   -> [Step 1] SHA-256 -> [Step 2] RSA-2048 서명 검증
 *  3. 통과 시 Stage 2 로 점프 / 실패 시 halt (recovery fallback 없음)
 *
 * Stage 1 은 update 불가 — 검증 로직 자체가 위조되면 trust chain 의
 * root 가 무너지므로 immutable 이어야 한다.
 */

#define STAGE2_BASE 0x60008000

/* Stage 2 image header (vector[7..9] 활용). 호스트 서명 도구와 약속 */
#define IMG_MAGIC_OFFSET 0x1Cu
#define IMG_SIZE_OFFSET 0x20u
#define IMG_CRC_OFFSET 0x24u
#define IMG_MAGIC_VALUE 0xDEADBEEFu

/* Stage 2 영역 size 한계 (0x60008000 ~ 0x60048000 = 256KB) */
#define STAGE2_SIZE_MIN 0x100u
#define STAGE2_SIZE_MAX 0x40000u

static void Jump_To_Stage2(uint32_t addr)
{
    uint32_t app_msp = *(volatile uint32_t *)addr;
    uint32_t app_pc = *(volatile uint32_t *)(addr + 4);

    __asm volatile("cpsid i"); /* 인터럽트 비활성화 */
    __asm volatile("dsb");     /* 메모리 접근 완료 대기 */
    __asm volatile("isb");     /* 파이프라인 초기화 */

    SCB_VTOR = addr; /*VTOR을 Stage 2 Vector Table 주소로 설정*/

    __asm volatile("dsb");
    __asm volatile("isb");

    __asm volatile(
        "msr msp, %0\n" /*MSP를 Stage 2 MSP로 설정*/
        "bx %1\n"       /*Stage 2 Reset_Handler 로 점프*/
        ::"r"(app_msp),
        "r"(app_pc));
}

/* digest 32byte를 16진수의 문자열로 출력 */
static void print_digest_hex(const uint8_t digest[SHA256_DIGEST_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    char buf[3];
    buf[2] = '\0';
    for (uint32_t i = 0; i < SHA256_DIGEST_SIZE; ++i)
    {
        buf[0] = hex[digest[i] >> 4];    // 상위 4비트
        buf[1] = hex[digest[i] & 0x0Fu]; // 하위 4비트
        UART1_SendString(buf);
    }
    UART1_SendString("\r\n");
}

/*
 * SHA-256 셀프테스트 — 답이 정해진 3개 벡터 해시·출력.
 * 호스트의 sha256sum 과 비교해 정답이면 우리 구현이 정확하다는 증명.
 */
static void sha256_selftest(void)
{
    uint8_t digest[SHA256_DIGEST_SIZE];

    UART1_SendString("[SHA] Self-test running...\r\n");

    /* 벡터 1: 빈 문자열 */
    SHA256_Compute((const uint8_t *)"", 0, digest);
    UART1_SendString("[SHA] empty   -> ");
    print_digest_hex(digest);

    /* 벡터 2: "abc" */
    SHA256_Compute((const uint8_t *)"abc", 3, digest);
    UART1_SendString("[SHA] \"abc\"   -> ");
    print_digest_hex(digest);

    /* 벡터 3: 56-byte (패딩 edge case) */
    static const char msg3[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    SHA256_Compute((const uint8_t *)msg3, 56, digest);
    UART1_SendString("[SHA] 56-byte -> ");
    print_digest_hex(digest);

    UART1_SendString("[SHA] Self-test done.\r\n\r\n");
}

/*
 * Secure Boot 의 FAIL 경로 = halt. recovery fallback 없음.
 * 검증에 실패한(위조됐을 수 있는) image 로는 절대 점프하지 않는다 —
 * 이것이 secure boot 의 핵심 불변식이다.
 * LED 상시 점등 = halt 상태 (정상 부팅 시 Stage 2 깜빡임과 구분).
 */
static void Halt_On_Verify_Fail()
{
    LED_On();
    __asm volatile("cpsid i");
    while (1)
    {
        __asm volatile("wfi");
    }
}

static int Stage2_VectorSane(uint32_t addr)
{
    uint32_t sp = *(volatile uint32_t *)addr;
    uint32_t pc = *(volatile uint32_t *)(addr + 4);

    /* SP 가 DTCM/OCRAM (0x2xxxxxxx) 안인지 */
    if ((sp & 0xF0000000u) != 0x20000000u)
        return 0;
    /* PC 가 FlexSPI FLASH (0x6xxxxxxx) 안인지 */
    if ((pc & 0xF0000000u) != 0x60000000u)
        return 0;
    /* PC 의 Thumb bit */
    if ((pc & 0x1u) != 0x1u)
        return 0;
    return 1;
}

static int Stage2_Verify(uint32_t base)
{
    /* Vector Sanity Check */
    if (!Stage2_VectorSane(base))
    {
        UART1_SendString("[Verify] vector sanity FAIL\r\n");
        return 0;
    }

    /* Magic Check */
    uint32_t magic = *(volatile uint32_t *)(base + IMG_MAGIC_OFFSET);
    if (magic != IMG_MAGIC_VALUE)
    {
        UART1_SendString("[Verify] magic value FAIL\r\n");
        return 0;
    }

    /* Size Check */
    uint32_t size = *(volatile uint32_t *)(base + IMG_SIZE_OFFSET);
    if (size < STAGE2_SIZE_MIN || size > STAGE2_SIZE_MAX)
    {
        UART1_SendString("[Verify] image size FAIL\r\n");
        return 0;
    }

    /* CRC32 Check */
    uint32_t expected_crc = *(volatile uint32_t *)(base + IMG_CRC_OFFSET);
    uint32_t computed_crc = CRC32_ComputeWithSkip(
        (const uint8_t *)base, size, IMG_CRC_OFFSET);

    if (computed_crc != expected_crc)
    {
        UART1_SendString("[Verify] CRC32 FAIL\r\n");
        return 0;
    }

    /**
     * SHA-256 - 이미지의 fingerprint 계산 후 UART 출력
     */

    uint8_t img_hash[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)base, size, img_hash);
    UART1_SendString("[Verify] SHA-256: ");
    print_digest_hex(img_hash);

    return 1; /* 검증 통과 */
}

static void bn_print_hex_be(const bn_t a)
{
    static const char hex[] = "0123456789abcdef";
    uint8_t buf[BN_BYTES];
    bn_to_bytes_be(buf, a);

    uint32_t start = 0;
    while (start < BN_BYTES - 1 && buf[start] == 0)
    {
        ++start;
    }

    char out[3] = {
        0,
    };
    for (uint32_t i = start; i < BN_BYTES; ++i)
    {
        out[0] = hex[(buf[i] >> 4) & 0xF];
        out[1] = hex[(buf[i] & 0xF)];
        UART1_SendString(out);
    }
    UART1_SendString("\r\n");
}

static void bignum_selftest(void)
{
    bn_t a, b, r;
    uint32_t carry;

    UART1_SendString("[BN] Self-test running...\r\n");

    /* 테스트 1: 단순 덧셈 (carry 없음)
        a = 1, b = 2, expect r = 3, carry_out = 0 */
    bn_zero(a);
    a[0] = 1;

    bn_zero(b);
    b[0] = 2;

    carry = bn_add(r, a, b);
    UART1_SendString("[BN] add 1+2: ");
    UART1_SendString(r[0] == 3 && carry == 0 ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 2: word 경계 carry 전파
        a = 0xFFFFFFFF, b = 1, expect r[0] = 0, r[1] = 1, carry_out = 0 */
    bn_zero(a);
    a[0] = 0xFFFFFFFFu;

    bn_zero(b);
    b[0] = 1;
    carry = bn_add(r, a, b);

    UART1_SendString("[BN] add carry: ");
    UART1_SendString((r[0] == 0 && r[1] == 1 && carry == 0) ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 3: top-word overflow
        a = b = 모든 워드가 0xFFFFFFFF, expect 최종 carry_out = 1 */
    for (int i = 0; i < BN_WORDS; ++i)
    {
        a[i] = 0xFFFFFFFFu;
        b[i] = 0;
    }
    b[0] = 1;
    carry = bn_add(r, a, b); /* (2^2048 - 1) + 1 = 2^2048, overflow */

    UART1_SendString("[BN] add overflow: ");
    UART1_SendString((carry == 1 && bn_is_zero(r)) ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 4: sub borrow
        a = [0, 1, 0, ...] (즉 2^32), b = 1, expect r = 0xFFFFFFFF, borrow_out = 0 */
    bn_zero(a);
    a[1] = 1;
    bn_zero(b);
    b[0] = 1;
    uint32_t borrow = bn_sub(r, a, b);

    UART1_SendString("[BN] sub borrow: ");
    UART1_SendString((r[0] == 0xFFFFFFFFu && r[1] == 0 && borrow == 0) ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 5: a - a = 0 */
    for (int i = 0; i < BN_WORDS; ++i)
    {
        a[i] = 0xDEADBEEFu;
    }

    borrow = bn_sub(r, a, a);

    UART1_SendString("[BN] sub self: ");
    UART1_SendString((bn_is_zero(r) && borrow == 0) ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 6: cmp */
    bn_zero(a);
    a[5] = 100;

    bn_zero(b);
    b[5] = 100;

    int c0 = bn_cmp(a, b); /* 0 */
    bn_zero(b);
    b[5] = 99;

    int c1 = bn_cmp(a, b); /* +1 (a>b) */
    bn_zero(b);
    b[5] = 101;

    int c2 = bn_cmp(a, b); /* -1 (a<b) */

    UART1_SendString("[BN] cmp: ");
    UART1_SendString((c0 == 0 && c1 > 0 && c2 < 0) ? "PASS\r\n" : "FAIL\r\n");

    /* 테스트 7: byte roundtrip
        원래 = [0x01, 0x02, ..., 0xFF, 0x00] 256바이트
        from_bytes_be → to_bytes_be 결과 == 원본 */
    uint8_t in[BN_BYTES], out[BN_BYTES];
    for (uint32_t i = 0; i < BN_BYTES; ++i)
    {
        in[i] = (uint8_t)(i + 1); /* 1, 2, ... */
    }

    bn_from_bytes_be(a, in);
    bn_to_bytes_be(out, a);
    int eq = 1;
    for (uint32_t i = 0; i < BN_BYTES; ++i)
    {
        if (in[i] != out[i])
        {
            eq = 0;
            break;
        }
    }

    UART1_SendString("[BN] byte roundtrip: ");
    UART1_SendString(eq ? "PASS\r\n" : "FAIL\r\n");

    /* 추가 보너스 — byte conversion 검증의 핵심:
        in[0] = 0x01 = MSB → a[63] 의 최상위 바이트로 들어가야
        그러므로 a[63] 의 상위 8비트 = 0x01 인지 확인 가능 */
    UART1_SendString("[BN] BE check: ");
    UART1_SendString(((a[BN_WORDS - 1] >> 24) == 0x01u) ? "PASS\r\n" : "FAIL\r\n");

    /* sample print — bn_print_hex_be 가 잘 동작하는지 시각 확인용 */
    bn_zero(a);
    a[0] = 0xDEADBEEF;
    a[1] = 0xCAFEBABE;
    UART1_SendString("[BN] sample = ");
    bn_print_hex_be(a); /* expect: cafebabedeadbeef  (옵션 B) 또는 256바이트 0-padded (옵션 A) */


    /* === bn_mul tests === */
    bn2_t prod;

    /* M1: 1 * 1 = 1 */
    bn_zero(a);
    a[0] = 1;
    bn_zero(b);
    b[0] = 1;

    bn_mul(prod, a, b);
    int m1_ok = (prod[0] == 1);

    for (uint32_t i = 1; i < BN_WORDS_2X; ++i)
    {
        if (prod[i])
        {
            m1_ok = 0;
            break;
        }
    }

    UART1_SendString("[BN] mul 1*1: ");
    UART1_SendString(m1_ok ? "PASS\r\n" : "FAIL\r\n");

    /* M2: 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001
            prod[0] = 0x00000001, prod[1] = 0xFFFFFFFE */
    bn_zero(a);
    a[0] = 0xFFFFFFFFu;
    bn_zero(b);
    b[0] = 0xFFFFFFFFu;

    bn_mul(prod, a, b);

    UART1_SendString("[BN] mul FFFF*FFFF: ");
    UART1_SendString((prod[0] == 0x00000001u && prod[1] == 0xFFFFFFFEu) ? "PASS\r\n" : "FAIL\r\n");

    /* M3: cross-word — a = 2^32 (a[1]=1), b = 3 (b[0]=3)
            expect prod[1] = 3, 나머지 0 */
    bn_zero(a);
    a[1] = 1;
    bn_zero(b);
    b[0] = 3;

    bn_mul(prod, a, b);

    UART1_SendString("[BN] mul cross-word: ");
    UART1_SendString((prod[0] == 0 && prod[1] == 3 && prod[2] == 0) ? "PASS\r\n" : "FAIL\r\n");

    /* M4: top-word — a[63] = 0xFFFFFFFF, b[63] = 0xFFFFFFFF
            expect prod[126] = 0x00000001, prod[127] = 0xFFFFFFFE */
    bn_zero(a);
    a[63] = 0xFFFFFFFFu;
    bn_zero(b);
    b[63] = 0xFFFFFFFFu;

    bn_mul(prod, a, b);

    UART1_SendString("[BN] mul top*top: ");
    UART1_SendString((prod[126] == 0x00000001u && prod[127] == 0xFFFFFFFEu) ? "PASS\r\n" : "FAIL\r\n");

    /* M5: zero — a * 0 = 0 */
    for (uint32_t i = 0; i < BN_WORDS; ++i)
        a[i] = 0xDEADBEEFu;
    bn_zero(b);
    bn_mul(prod, a, b);

    int m5_ok = 1;

    for (uint32_t i = 0; i < BN_WORDS_2X; ++i)
    {
        if (prod[i])
        {
            m5_ok = 0;
            break;
        }
    }

    UART1_SendString("[BN] mul x*0: ");
    UART1_SendString(m5_ok ? "PASS\r\n" : "FAIL\r\n");

    /* M6: commutative — a*b == b*a */
    bn_zero(a);
    a[0] = 0x12345678u;
    a[3] = 0xABCDEF01u;

    bn_zero(b);
    b[0] = 0x9ABCDEF0u;
    b[5] = 0x11223344u;

    bn2_t prod2;
    bn_mul(prod, a, b);
    bn_mul(prod2, b, a);

    int m6_ok = 1;
    for (uint32_t i = 0; i < BN_WORDS_2X; ++i)
    {
        if (prod[i] != prod2[i])
        {
            m6_ok = 0;
            break;
        }
    }

    UART1_SendString("[BN] mul commutative: ");
    UART1_SendString(m6_ok ? "PASS\r\n" : "FAIL\r\n");

    UART1_SendString("[BN] Self-test done.\r\n\r\n");
}

int main()
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[BL1] Stage 1 immutable (Secure Boot)\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    sha256_selftest();
    bignum_selftest();

    UART1_SendString("[BL1] Verifying Stage 2 ...\r\n");
    if (Stage2_Verify(STAGE2_BASE))
    {
        UART1_SendString("[BL1] Stage 2 OK - jumping to 0x60008000\r\n");
        Jump_To_Stage2(STAGE2_BASE);
    }

    UART1_SendString("[BL1] Stage 2 INVALID - halting (no recovery)\r\n");
    Halt_On_Verify_Fail();

    /* unreachable */
    for (;;)
    {
        __asm("wfi");
    }
    return 0;
}