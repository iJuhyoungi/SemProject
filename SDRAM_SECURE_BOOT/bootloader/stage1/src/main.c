#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "rt1020_regs.h"
#include "crc32.h"
#include "sha256.h"
#include "bignum.h"
#include "rsa.h"
#include "embedded_pubkey.h"

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

    /*RSA-2048 PKCS#1 v1.5 서명 검증 - image 끝 256바이트 = signature*/
    const uint8_t *signature=(const uint8_t*)(base+size);
    if(!rsa_verify_pkcs1_v15_sha256(img_hash,signature,EMBEDDED_PUBKEY_MODULUS)){
        UART1_SendString("[Verify] RSA signature FAIL\r\n");
        return 0;
    }

    UART1_SendString("[Verify] RSA signature OK\r\n");
    return 1; /* 검증 통과 */
}


/**
 * board smoke test - host의 UT가 테스트를 담당하며,
 * 여기에서는 보드가 bring-up되고 핵심 함수가 호출되는지만 확인
 *
 * 각 알고리즘의 case 1만 PASS/FAIL로 출력한다.
 */
static void selftest_smoke(void)
{
    UART1_SendString("[Selftest] running smoke...\r\n");

    /* SHA-256: NIST "abc" — 가장 단순한 표준 vector */
    static const uint8_t sha_abc_expected[SHA256_DIGEST_SIZE] = {
        0xba,
        0x78,
        0x16,
        0xbf,
        0x8f,
        0x01,
        0xcf,
        0xea,
        0x41,
        0x41,
        0x40,
        0xde,
        0x5d,
        0xae,
        0x22,
        0x23,
        0xb0,
        0x03,
        0x61,
        0xa3,
        0x96,
        0x17,
        0x7a,
        0x9c,
        0xb4,
        0x10,
        0xff,
        0x61,
        0xf2,
        0x00,
        0x15,
        0xad,
    };
    uint8_t sha_digest[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)"abc", 3, sha_digest);
    int sha_ok = 1;
    for (uint32_t i = 0; i < SHA256_DIGEST_SIZE; ++i)
    {
        if (sha_digest[i] != sha_abc_expected[i])
        {
            sha_ok = 0;
            break;
        }
    }
    UART1_SendString("[Selftest] SHA-256(\"abc\")  : ");
    UART1_SendString(sha_ok ? "PASS\r\n" : "FAIL\r\n");

    /* bn_add: 1 + 2 = 3 */
    bn_t a, b, r;
    bn_zero(a);
    a[0] = 1;
    bn_zero(b);
    b[0] = 2;
    uint32_t carry = bn_add(r, a, b);
    UART1_SendString("[Selftest] bn_add(1+2)     : ");
    UART1_SendString((r[0] == 3 && carry == 0) ? "PASS\r\n" : "FAIL\r\n");

    /* bn_mul: 1 * 1 = 1 */
    bn2_t prod;
    bn_zero(a);
    a[0] = 1;
    bn_zero(b);
    b[0] = 1;
    bn_mul(prod, a, b);
    UART1_SendString("[Selftest] bn_mul(1*1)     : ");
    UART1_SendString((prod[0] == 1) ? "PASS\r\n" : "FAIL\r\n");

    /* bn_mod : 5 mod 3 = 2 */
    bn_t mod_r;
    bn2_t five;
    bn2_zero(five);
    five[0]=5;
    
    bn_zero(a);
    a[0]=3;

    bn_mod(mod_r,five,a);
    UART1_SendString("[Selftest] bn_mod(5 mod 3) : ");
    UART1_SendString((mod_r[0] == 2) && (mod_r[1] == 0) ? "PASS\r\n" : "FAIL\r\n");

    /*bn_modexp: 2^10 mod 1000 = 24*/
    bn_t base, mod, exp_r;
    bn_zero(base);
    base[0]=2;

    bn_zero(mod);
    mod[0]=1000;

    bn_modexp(exp_r, base, 10u, mod);
    UART1_SendString("[Selftest] bn_modexp(2^10%1000): ");
    UART1_SendString((exp_r[0] == 24 && exp_r[1] == 0) ? "PASS\r\n" : "FAIL\r\n");

    UART1_SendString("[Selftest] done.\r\n\r\n");
}

int main()
{
    UART1_SendString("\r\n=============================\r\n");
    UART1_SendString("[BL1] Stage 1 immutable (Secure Boot)\r\n");
    UART1_SendString("=============================\r\n");

    LED_Init();

    selftest_smoke();

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