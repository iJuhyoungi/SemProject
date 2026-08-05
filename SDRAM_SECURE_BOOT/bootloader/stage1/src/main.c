#include <stdint.h>
#include "uart.h"
#include "led.h"
// #include "rt1020_regs.h"
// #include "crc32.h"
#include "sha256.h"
#include "bignum.h"
// #include "rsa.h"
#include "verify.h"
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

static void Halt_On_Verify_Fail()
{
    LED_On();
    __asm volatile("cpsid i");
    while (1)
    {
        __asm volatile("wfi");
    }
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

    uint8_t s2_digest[SHA256_DIGEST_SIZE];
    sec_bool_t v1 = verify_image(STAGE2_BASE, EMBEDDED_ROOT_MODULUS, s2_digest);
    sec_bool_t v2 = verify_image(STAGE2_BASE, EMBEDDED_ROOT_MODULUS, s2_digest);

    if(v1!=v2){
        UART1_SendString("[BL1] verdict mismatch between two runs - rejecting\r\n");
    }
    else if (SEC_IS_PASS(v1) && SEC_IS_PASS(v2))
    {
        UART1_SendString("[BL1] Stage 2 OK - jumping to 0x60008000\r\n");
        jump_to_image(STAGE2_BASE, v1);
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