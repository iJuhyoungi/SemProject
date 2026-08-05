#include <stdint.h>
#include "uart.h"
#include "led.h"
#include "bignum.h"
#include "verify.h"
#include "metadata.h"
#include "embedded_pubkey.h"
#include "keycert.h"
#include "glitch.h"

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

static const char *keycert_reason_str(keycert_reason_t r)
{
    switch (r)
    {
    case KEYCERT_OK:
        return "OK";
    case KEYCERT_BAD_MAGIC:
        return "BAD_MAGIC";
    case KEYCERT_BAD_SIGNATURE:
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
    GLITCH_BANNER();

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
    sec_bool_t ok = metadata_read_active(&md, &p_reason, &b_reason);

    UART1_SendString("[BL2] Metadata Primary: ");
    UART1_SendString(metadata_reason_str(p_reason));
    UART1_SendString("\r\n");
    UART1_SendString("[BL2] Metadata Backup:  ");
    UART1_SendString(metadata_reason_str(b_reason));
    UART1_SendString("\r\n");

    if (!SEC_IS_PASS(ok))
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

    /*
     * Key certificate — App 서명 검증에 쓸 release 공개키를 가져옵니다.
     * 인증서 자체는 root 키로 검증하므로, 공격자가 자기 공개키로 바꿔치기해도
     * root 서명을 만들 수 없어 여기서 걸립니다.
     */
    bn_t release_modulus;
    uint32_t key_id = 0, key_version = 0;
    keycert_reason_t kc_reason;
    sec_bool_t kc_ok = keycert_load(release_modulus, &key_id, &key_version, &kc_reason);

    UART1_SendString("[BL2] Key cert: ");
    UART1_SendString(keycert_reason_str(kc_reason));
    UART1_SendString("\r\n");

    if (!SEC_IS_PASS(kc_ok))
    {
        UART1_SendString("[BL2] Key certificate invalid - halting (fail-safe)\r\n");
        halt_on_fail();
    }

    UART1_SendString("[BL2] Key id = ");
    print_hex32(key_id);
    UART1_SendString("  version = ");
    print_hex32(key_version);
    UART1_SendString("\r\n");

    /*priority check*/
    uint32_t primary, secondary;
    uint32_t prim_ver, sec_ver;
    const char *prim_name, *sec_name;
    const uint8_t *prim_digest, *sec_digest;
    if (vb > va)
    {
        primary = APP_B_BASE;
        prim_name = "App B";
        prim_ver = vb;
        prim_digest = md.app_b_digest;
        secondary = APP_A_BASE;
        sec_name = "App A";
        sec_ver = va;
        sec_digest = md.app_a_digest;
    }
    else
    {
        primary = APP_A_BASE;
        prim_name = "App A";
        prim_ver = va;
        prim_digest = md.app_a_digest;
        secondary = APP_B_BASE;
        sec_name = "App B";
        sec_ver = vb;
        sec_digest = md.app_b_digest;
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
    else {
        uint8_t d1[SHA256_DIGEST_SIZE], d2[SHA256_DIGEST_SIZE];

        sec_bool_t v1 = verify_image(primary, release_modulus, d1);
        sec_bool_t v2 = verify_image(primary, release_modulus, d2);

        v1 = (sec_bool_t)glitch_bitflip((uint32_t)v1);

        if (v1 != v2)
        {
            UART1_SendString("[BL2] verdict mismatch between two runs - rejecting\r\n");
        }
        /*
         * 서명 판정을 측정값보다 먼저 봅니다. 서명이 깨진 이미지의 측정값을
         * 따지는 것은 의미가 없고, 진단 메시지가 실제 원인을 가리켜야 합니다.
         *
         * 글리치 ② 는 이 분기가 실행되지 않는 상황을 모사합니다. 조건을 억지로
         * 참으로 만드는 게 아니라 분기 자체를 건너뛰게 하는 형태라 실제 명령
         * 스킵에 더 가깝습니다. 건너뛰면 아래 검사들로 흘러가고, 마지막에는
         * jump_to_image 의 재판정이 막습니다.
         */
        else if (!GLITCH_SKIP_BRANCH_TAKEN()
                 && (!SEC_IS_PASS(v1) || !SEC_IS_PASS(v2)))
        {
            UART1_SendString("[BL2] ");
            UART1_SendString(prim_name);
            UART1_SendString(" REJECTED - signature verdict not PASS\r\n");
        }
        else if (!SEC_IS_PASS(sec_memeq(d1, d2, SHA256_DIGEST_SIZE)))
        {
            UART1_SendString("[BL2] measurement mismatch between two runs - rejecting\r\n");
        }
        else if (!SEC_IS_PASS(sec_memeq(d1, prim_digest, SHA256_DIGEST_SIZE)))
        {
            UART1_SendString("[BL2] ");
            UART1_SendString(prim_name);
            UART1_SendString(" REJECTED - measurement does not match policy\r\n");
        }
        else
        {
            /* 여기 도달했다는 것은 위 판정 분기가 이미 v1·v2 를 통과시켰다는 뜻입니다.
             * 같은 조건을 바로 옆줄에서 또 쓰면 두 분기 명령이 인접해 한 번의
             * 글리치에 함께 날아갈 수 있습니다. 두 번째 확인은 다른 함수인
             * jump_to_image 안, 돌이킬 수 없는 지점 직전에 있습니다. */
            UART1_SendString("[BL2] ");
            UART1_SendString(prim_name);
            UART1_SendString(" OK - jumping\r\n");
            jump_to_image(primary, v1);
        }
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
    // else if (verify_image(secondary, EMBEDDED_ROOT_MODULUS))
    // else if(SEC_IS_PASS(verify_image(secondary, EMBEDDED_ROOT_MODULUS)))
    else
    {
        uint8_t sd1[SHA256_DIGEST_SIZE], sd2[SHA256_DIGEST_SIZE];
        sec_bool_t s1 = verify_image(secondary, release_modulus, sd1);
        sec_bool_t s2 = verify_image(secondary, release_modulus, sd2);

        if (s1 != s2)
        {
            UART1_SendString("[BL2] verdict mismatch between two runs - rejecting\r\n");
        }
        else if (!SEC_IS_PASS(s1) || !SEC_IS_PASS(s2))
        {
            UART1_SendString("[BL2] ");
            UART1_SendString(sec_name);
            UART1_SendString(" REJECTED - signature verdict not PASS\r\n");
        }
        else if (!SEC_IS_PASS(sec_memeq(sd1, sd2, SHA256_DIGEST_SIZE)))
        {
            UART1_SendString("[BL2] measurement mismatch between two runs - rejecting\r\n");
        }
        else if (!SEC_IS_PASS(sec_memeq(sd1, sec_digest, SHA256_DIGEST_SIZE)))
        {
            UART1_SendString("[BL2] ");
            UART1_SendString(sec_name);
            UART1_SendString(" REJECTED - measurement does not match policy\r\n");
        }
        else
        {
            UART1_SendString("[BL2] ");
            UART1_SendString(sec_name);
            UART1_SendString(" OK - jumping\r\n");
            jump_to_image(secondary, s1);
        }
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
