#include "unity.h"
#include "secure.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static int popcount32(uint32_t v)
{
    int n = 0;
    while (v)
    {
        n += (int)(v & 1u);
        v >>= 1;
    }
    return n;
}

void test_tokens_are_hamming_distant(void)
{
    TEST_ASSERT_EQUAL_INT(32, popcount32(SEC_PASS ^ SEC_FAIL));
}

void test_tokens_are_far_from_degenerate_values(void)
{
    TEST_ASSERT_GREATER_OR_EQUAL_INT(8, popcount32(SEC_PASS ^ 0x00000000u));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(8, popcount32(SEC_PASS ^ 0xFFFFFFFFu));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(8, popcount32(SEC_FAIL ^ 0x00000000u));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(8, popcount32(SEC_FAIL ^ 0xFFFFFFFFu));
}

void test_tokens_are_balanced(void)
{
    TEST_ASSERT_EQUAL_INT(16, popcount32(SEC_PASS));
    TEST_ASSERT_EQUAL_INT(16, popcount32(SEC_FAIL));
}

void test_is_pass_accepts_only_the_pass_token(void)
{
    TEST_ASSERT_TRUE(SEC_IS_PASS(SEC_PASS));
    TEST_ASSERT_FALSE(SEC_IS_PASS(SEC_FAIL));
    TEST_ASSERT_FALSE(SEC_IS_PASS(0x00000000u));
    TEST_ASSERT_FALSE(SEC_IS_PASS(0xFFFFFFFFu));
    TEST_ASSERT_FALSE(SEC_IS_PASS(1u));
}

void test_single_bit_flip_never_yields_pass(void)
{
    for (int i = 0; i < 32; ++i)
    {
        TEST_ASSERT_FALSE(SEC_IS_PASS(SEC_FAIL ^ (1u << i)));
        TEST_ASSERT_FALSE(SEC_IS_PASS(0x00000000u ^ (1u << i)));
        TEST_ASSERT_FALSE(SEC_IS_PASS(0xFFFFFFFFu ^ (1u << i)));
    }
}

void test_memeq_equal_buffers(void)
{
    uint8_t a[32], b[32];
    memset(a, 0x5A, sizeof(a));
    memset(b, 0x5A, sizeof(b));
    TEST_ASSERT_TRUE(SEC_IS_PASS(sec_memeq(a, b, sizeof(a))));
}

void test_memeq_detects_difference_at_any_position(void)
{
    uint8_t a[32], b[32];
    for (size_t pos = 0; pos < sizeof(a); ++pos)
    {
        memset(a, 0x5A, sizeof(a));
        memset(b, 0x5A, sizeof(b));
        b[pos] ^= 0x01; /* 한 바이트의 1비트만 다르게 */
        TEST_ASSERT_FALSE(SEC_IS_PASS(sec_memeq(a, b, sizeof(a))));

        b[pos] ^= 0x01; /* 원복 */
        b[pos] ^= 0x80;
        TEST_ASSERT_FALSE(SEC_IS_PASS(sec_memeq(a, b, sizeof(a))));
    }
}

void test_memeq_zero_length_is_equal(void)
{
    TEST_ASSERT_TRUE(SEC_IS_PASS(sec_memeq("a", "b", 0)));
}

void test_loop_check(void)
{
    TEST_ASSERT_TRUE(SEC_IS_PASS(SEC_LOOP_CHECK(202u, 202u)));
    TEST_ASSERT_FALSE(SEC_IS_PASS(SEC_LOOP_CHECK(0u, 202u)));   /* 루프 통째 스킵 */
    TEST_ASSERT_FALSE(SEC_IS_PASS(SEC_LOOP_CHECK(201u, 202u))); /* 한 바퀴 모자람 */
}

void test_memeq_covers_all_byte_differences(void)
{
    for (int v = 0; v < 256; ++v)
    {
        uint8_t a = 0x00;
        uint8_t b = (uint8_t)v;
        sec_bool_t r = sec_memeq(&a, &b, 1);

        if (v == 0)
        {
            TEST_ASSERT_TRUE(SEC_IS_PASS(r));
        }
        else
        {
            TEST_ASSERT_FALSE(SEC_IS_PASS(r));
            TEST_ASSERT_EQUAL_HEX32(SEC_FAIL, r); /* 쓰레기 값이 아니라 정확히 FAIL */
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tokens_are_hamming_distant);
    RUN_TEST(test_tokens_are_far_from_degenerate_values);
    RUN_TEST(test_tokens_are_balanced);
    RUN_TEST(test_is_pass_accepts_only_the_pass_token);
    RUN_TEST(test_single_bit_flip_never_yields_pass);
    RUN_TEST(test_memeq_equal_buffers);
    RUN_TEST(test_memeq_detects_difference_at_any_position);
    RUN_TEST(test_memeq_covers_all_byte_differences);
    RUN_TEST(test_memeq_zero_length_is_equal);
    RUN_TEST(test_loop_check);


    return UNITY_END();
}
