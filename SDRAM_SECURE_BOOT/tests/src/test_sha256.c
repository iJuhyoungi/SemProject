#include "unity.h"
#include "sha256.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ========================================================
* NIST FIPS 180-4 standard vectors (정답 알려진 표준)
* ======================================================== */

/* empty string */
void test_sha256_empty_string(void)
{
    static const uint8_t expected[SHA256_DIGEST_SIZE] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
    };
    uint8_t digest[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)"", 0, digest);
    TEST_ASSERT_EQUAL_MEMORY(expected, digest, SHA256_DIGEST_SIZE);
}

/* NIST FIPS 180-4 Appendix B.1 — "abc" (3 bytes, single block, no padding extra block) */
void test_sha256_nist_abc(void)
{ 
    static const uint8_t expected[SHA256_DIGEST_SIZE] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    uint8_t digest[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)"abc", 3, digest);
    TEST_ASSERT_EQUAL_MEMORY(expected, digest, SHA256_DIGEST_SIZE);
}

/* NIST FIPS 180-4 Appendix B.2 — 56 bytes (padding spills into 2nd block) */
void test_sha256_nist_56_byte_padding_spill(void)
{ 
    static const char msg[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static const uint8_t expected[SHA256_DIGEST_SIZE] = {
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
        0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
        0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1,
    };
    uint8_t digest[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)msg, 56, digest);
    TEST_ASSERT_EQUAL_MEMORY(expected, digest, SHA256_DIGEST_SIZE);
}

/* ========================================================
* Streaming API 동등성 (Compute vs Init/Update/Final)
*   — 어떤 호출 방식으로도 결과가 같아야 SHA API 가 올바르게 구현되었다는 증거
* ======================================================== */

/* Init/Update/Final 분리 호출 == Compute 단일 호출 */
void test_sha256_streaming_equivalent_to_compute(void)
{
    const uint8_t msg[] = "abc";
    uint8_t digest_compute[SHA256_DIGEST_SIZE];
    uint8_t digest_streaming[SHA256_DIGEST_SIZE];

    SHA256_Compute(msg, 3, digest_compute);

    sha256_ctx ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, msg, 3);
    SHA256_Final(&ctx, digest_streaming);

    TEST_ASSERT_EQUAL_MEMORY(digest_compute, digest_streaming, SHA256_DIGEST_SIZE);
}

/* Multi-update: "ab" + "c" 와 "abc" 단일 입력 결과 동일 */
void test_sha256_multi_update_equivalent_to_single(void)
{ 
    uint8_t digest_split[SHA256_DIGEST_SIZE];
    uint8_t digest_single[SHA256_DIGEST_SIZE];

    sha256_ctx ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, (const uint8_t *)"ab", 2);
    SHA256_Update(&ctx, (const uint8_t *)"c", 1);
    SHA256_Final(&ctx, digest_split);

    SHA256_Compute((const uint8_t *)"abc", 3, digest_single);

    TEST_ASSERT_EQUAL_MEMORY(digest_single, digest_split, SHA256_DIGEST_SIZE);
}

/* Byte-by-byte update (1바이트씩 100회) == 한 번에 100바이트 입력 결과 동일
    — 누적 상태가 어떻게 흘려도 일관성 유지하는지 확인 */
void test_sha256_byte_by_byte_update_equivalent_to_bulk(void)
{
    uint8_t msg[100];
    for (uint32_t i = 0; i < 100; ++i) msg[i] = (uint8_t)i;

    uint8_t digest_split[SHA256_DIGEST_SIZE];
    uint8_t digest_bulk[SHA256_DIGEST_SIZE];

    sha256_ctx ctx;
    SHA256_Init(&ctx);
    for (uint32_t i = 0; i < 100; ++i) {
        SHA256_Update(&ctx, &msg[i], 1);
    }
    SHA256_Final(&ctx, digest_split);

    SHA256_Compute(msg, 100, digest_bulk);

    TEST_ASSERT_EQUAL_MEMORY(digest_bulk, digest_split, SHA256_DIGEST_SIZE);
} 

/* ========================================================
* main
* ======================================================== */

int main(void)
{ 
    UNITY_BEGIN();

    /* NIST standard */
    RUN_TEST(test_sha256_empty_string);
    RUN_TEST(test_sha256_nist_abc);
    RUN_TEST(test_sha256_nist_56_byte_padding_spill);

    /* Streaming API 동등성 */
    RUN_TEST(test_sha256_streaming_equivalent_to_compute);
    RUN_TEST(test_sha256_multi_update_equivalent_to_single);
    RUN_TEST(test_sha256_byte_by_byte_update_equivalent_to_bulk);

    return UNITY_END();
}
