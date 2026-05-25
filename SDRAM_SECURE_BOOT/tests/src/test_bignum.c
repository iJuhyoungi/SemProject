#include "unity.h" 
#include "bignum.h"
#include "../vectors/bn_mod_vectors.h"
    
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
* mod (128 word a) mod (64 word n) = (64 word r)
*   reference: tests/vectors/gen_bn_mod_vectors.py 의 Python int 결과
* ======================================================== */

void test_bn_mod_v1_a_less_than_n_returns_a(void)
{
    bn_t r;
    // bn2_t a;
    // memcpy(a, v1_a_less_than_n_a, sizeof(bn2_t));
    // memcpy(n, v1_a_less_than_n_n, sizeof(bn_t));
    // bn_mod(r, a, n);
    bn_mod(r, v1_a_less_than_n_a, v1_a_less_than_n_n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(v1_a_less_than_n_r, r, BN_WORDS);
}

void test_bn_mod_v2_simple(void)
{ 
    bn_t r;
    // bn2_t a;
    // memcpy(a, v2_simple_a, sizeof(bn2_t));
    // memcpy(n, v2_simple_n, sizeof(bn_t));
    // bn_mod(r, a, n);
    bn_mod(r, v2_simple_a, v2_simple_n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(v2_simple_r, r, BN_WORDS);
}

void test_bn_mod_v3_large_a_small_n(void)
{ 
    bn_t r;
    // bn2_t a;
    // memcpy(a, v3_large_a_small_n_a, sizeof(bn2_t));
    // memcpy(n, v3_large_a_small_n_n, sizeof(bn_t));
    // bn_mod(r, a, n);
    bn_mod(r, v3_large_a_small_n_a, v3_large_a_small_n_n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(v3_large_a_small_n_r, r, BN_WORDS);
}

void test_bn_mod_v4_rsa_size(void)
{ 
    bn_t r;
    // bn2_t a;
    // memcpy(a, v4_rsa_size_a, sizeof(bn2_t));
    // memcpy(n, v4_rsa_size_n, sizeof(bn_t));
    // bn_mod(r, a, n);
    bn_mod(r, v4_rsa_size_a, v4_rsa_size_n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(v4_rsa_size_r, r, BN_WORDS);
}

void test_bn_mod_v5_exactly_2n_returns_zero(void)
{
    bn_t r; 
    // bn2_t a; 
    // memcpy(a, v5_exactly_2n_a, sizeof(bn2_t));
    // memcpy(n, v5_exactly_2n_n, sizeof(bn_t));
    // bn_mod(r, a, n);
    bn_mod(r, v5_exactly_2n_a, v5_exactly_2n_n);
    /* 2n mod n = 0 */
    TEST_ASSERT_EQUAL_UINT32_ARRAY(v5_exactly_2n_r, r, BN_WORDS);
    TEST_ASSERT_TRUE(bn_is_zero(r));
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

    /* mod */
    RUN_TEST(test_bn_mod_v1_a_less_than_n_returns_a);
    RUN_TEST(test_bn_mod_v2_simple);
    RUN_TEST(test_bn_mod_v3_large_a_small_n);
    RUN_TEST(test_bn_mod_v4_rsa_size);
    RUN_TEST(test_bn_mod_v5_exactly_2n_returns_zero);

    return UNITY_END();
}

