#include "unity.h" 
#include "bignum.h"
    
void setUp(void) {}
void tearDown(void) {}

/* ========================================================
* add
* ======================================================== */

void test_bn_add_one_plus_two_equals_three(void)
{
    bn_t a, b, r;
    bn_zero(a); a[0] = 1;
    bn_zero(b); b[0] = 2;
    uint32_t carry = bn_add(r, a, b);
    TEST_ASSERT_EQUAL_UINT32(0, carry);
    TEST_ASSERT_EQUAL_UINT32(3, r[0]);
}

void test_bn_add_propagates_carry_across_word_boundary(void)
{ 
    bn_t a, b, r;
    bn_zero(a); a[0] = 0xFFFFFFFFu;
    bn_zero(b); b[0] = 1;
    uint32_t carry = bn_add(r, a, b);
    TEST_ASSERT_EQUAL_UINT32(0, r[0]);
    TEST_ASSERT_EQUAL_UINT32(1, r[1]);
    TEST_ASSERT_EQUAL_UINT32(0, carry); 
}

void test_bn_add_top_word_overflow_returns_carry(void)
{ 
    bn_t a, b, r;
    for (uint32_t i = 0; i < BN_WORDS; ++i) {
        a[i] = 0xFFFFFFFFu;
        b[i] = 0;
    }
    b[0] = 1;
    uint32_t carry = bn_add(r, a, b);   /* (2^2048 - 1) + 1 = 2^2048 → overflow */
    TEST_ASSERT_EQUAL_UINT32(1, carry);
    TEST_ASSERT_TRUE(bn_is_zero(r));
}

/* ========================================================
* sub
* ======================================================== */

void test_bn_sub_borrows_across_word_boundary(void)
{
    bn_t a, b, r;
    bn_zero(a); a[1] = 1;               /* a = 2^32 */
    bn_zero(b); b[0] = 1;
    uint32_t borrow = bn_sub(r, a, b);
    TEST_ASSERT_EQUAL_UINT32(0, borrow);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, r[0]);
    TEST_ASSERT_EQUAL_UINT32(0, r[1]);
}

void test_bn_sub_self_yields_zero(void)
{
    bn_t a, r;
    for (uint32_t i = 0; i < BN_WORDS; ++i) {
        a[i] = 0xDEADBEEFu;
    }
    uint32_t borrow = bn_sub(r, a, a);
    TEST_ASSERT_EQUAL_UINT32(0, borrow);
    TEST_ASSERT_TRUE(bn_is_zero(r));
}

/* ========================================================
* cmp
* ======================================================== */

void test_bn_cmp_equal_returns_zero(void)
{
    bn_t a, b;
    bn_zero(a); a[5] = 100;
    bn_zero(b); b[5] = 100; 
    TEST_ASSERT_EQUAL_INT(0, bn_cmp(a, b));
}

void test_bn_cmp_greater_returns_positive(void)
{
    bn_t a, b;
    bn_zero(a); a[5] = 100;
    bn_zero(b); b[5] = 99;
    TEST_ASSERT_GREATER_THAN_INT(0, bn_cmp(a, b));
}

void test_bn_cmp_less_returns_negative(void)
{ 
    bn_t a, b;
    bn_zero(a); a[5] = 100;
    bn_zero(b); b[5] = 101;
    TEST_ASSERT_LESS_THAN_INT(0, bn_cmp(a, b));
} 

/* ========================================================
* bytes (BE byte ↔ LE word 변환)
* ======================================================== */

void test_bn_bytes_roundtrip_preserves_input(void)
{
    bn_t a;
    uint8_t in[BN_BYTES], out[BN_BYTES];
    for (uint32_t i = 0; i < BN_BYTES; ++i) {
        in[i] = (uint8_t)(i + 1);
    }
    bn_from_bytes_be(a, in);
    bn_to_bytes_be(out, a);
    TEST_ASSERT_EQUAL_MEMORY(in, out, BN_BYTES);
}

void test_bn_bytes_be_msb_lands_at_top_word(void)
{
    bn_t a;
    uint8_t in[BN_BYTES];
    for (uint32_t i = 0; i < BN_BYTES; ++i) {
        in[i] = (uint8_t)(i + 1);       /* in[0] = 0x01 = MSB */
    }
    bn_from_bytes_be(a, in);
    TEST_ASSERT_EQUAL_UINT32(0x01u, a[BN_WORDS - 1] >> 24);
}

/* ========================================================
* mul (64×64 → 128 워드)
* ======================================================== */

void test_bn_mul_one_times_one(void)
{
    bn_t a, b;
    bn2_t prod;
    bn_zero(a); a[0] = 1;
    bn_zero(b); b[0] = 1;
    bn_mul(prod, a, b);
    TEST_ASSERT_EQUAL_UINT32(1, prod[0]);
    TEST_ASSERT_EACH_EQUAL_UINT32(0, &prod[1], BN_WORDS_2X - 1);
}

void test_bn_mul_max_word_squared(void) 
{ 
    bn_t a, b;
    bn2_t prod;
    bn_zero(a); a[0] = 0xFFFFFFFFu;
    bn_zero(b); b[0] = 0xFFFFFFFFu;
    bn_mul(prod, a, b);
    /* (2^32 - 1)^2 = 0xFFFFFFFE_00000001 */
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, prod[0]);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFEu, prod[1]);
    TEST_ASSERT_EACH_EQUAL_UINT32(0, &prod[2], BN_WORDS_2X - 2);
}

void test_bn_mul_cross_word_alignment(void)
{
    bn_t a, b;
    bn2_t prod;
    bn_zero(a); a[1] = 1;               /* 2^32 */
    bn_zero(b); b[0] = 3;
    bn_mul(prod, a, b);
    TEST_ASSERT_EQUAL_UINT32(0, prod[0]);
    TEST_ASSERT_EQUAL_UINT32(3, prod[1]);
    TEST_ASSERT_EACH_EQUAL_UINT32(0, &prod[2], BN_WORDS_2X - 2);
}

void test_bn_mul_top_word_squared(void) 
{ 
    bn_t a, b;
    bn2_t prod;
    bn_zero(a); a[63] = 0xFFFFFFFFu;
    bn_zero(b); b[63] = 0xFFFFFFFFu;
    bn_mul(prod, a, b);
    TEST_ASSERT_EACH_EQUAL_UINT32(0, &prod[0], 126);
    TEST_ASSERT_EQUAL_UINT32(0x00000001u, prod[126]);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFEu, prod[127]);
}

void test_bn_mul_anything_times_zero(void)
{
    bn_t a, b;
    bn2_t prod;
    for (uint32_t i = 0; i < BN_WORDS; ++i) a[i] = 0xDEADBEEFu;
    bn_zero(b);
    bn_mul(prod, a, b);
    TEST_ASSERT_EACH_EQUAL_UINT32(0, prod, BN_WORDS_2X);
}

void test_bn_mul_is_commutative(void)
{ 
    bn_t a, b;
    bn2_t prod1, prod2;
    bn_zero(a); a[0] = 0x12345678u; a[3] = 0xABCDEF01u;
    bn_zero(b); b[0] = 0x9ABCDEF0u; b[5] = 0x11223344u;
    bn_mul(prod1, a, b);
    bn_mul(prod2, b, a);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(prod1, prod2, BN_WORDS_2X);
}

/* ========================================================
* main 
* ======================================================== */

int main(void)
{
    UNITY_BEGIN();

    /* add */
    RUN_TEST(test_bn_add_one_plus_two_equals_three);
    RUN_TEST(test_bn_add_propagates_carry_across_word_boundary);
    RUN_TEST(test_bn_add_top_word_overflow_returns_carry);

    /* sub */
    RUN_TEST(test_bn_sub_borrows_across_word_boundary);
    RUN_TEST(test_bn_sub_self_yields_zero);

    /* cmp */
    RUN_TEST(test_bn_cmp_equal_returns_zero);
    RUN_TEST(test_bn_cmp_greater_returns_positive);
    RUN_TEST(test_bn_cmp_less_returns_negative);

    /* bytes */
    RUN_TEST(test_bn_bytes_roundtrip_preserves_input);
    RUN_TEST(test_bn_bytes_be_msb_lands_at_top_word);

    /* mul */
    RUN_TEST(test_bn_mul_one_times_one);
    RUN_TEST(test_bn_mul_max_word_squared);
    RUN_TEST(test_bn_mul_cross_word_alignment);
    RUN_TEST(test_bn_mul_top_word_squared);
    RUN_TEST(test_bn_mul_anything_times_zero);
    RUN_TEST(test_bn_mul_is_commutative);

    return UNITY_END();
}

