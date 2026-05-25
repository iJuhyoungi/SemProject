#include "unity.h"
#include "rsa.h" 
#include "bignum.h"

#include "../vectors/rsa_test_vectors.h"

void setUp(void) {}
void tearDown(void) {}

/* === Valid signatures must PASS === */

void test_rsa_verify_v1_hello_world(void)
{
    TEST_ASSERT_EQUAL_INT(1,
        rsa_verify_pkcs1_v15_sha256(v1_hash, v1_signature, rsa_test_modulus));
}

void test_rsa_verify_v2_64kb_image(void)
{
    TEST_ASSERT_EQUAL_INT(1,
        rsa_verify_pkcs1_v15_sha256(v2_hash, v2_signature, rsa_test_modulus));
}

void test_rsa_verify_v3_empty_input(void)
{
    TEST_ASSERT_EQUAL_INT(1,
        rsa_verify_pkcs1_v15_sha256(v3_hash, v3_signature, rsa_test_modulus));
}

/* === Tampered signatures must FAIL === */

void test_rsa_verify_v4_tampered_signature_fails(void)
{
    TEST_ASSERT_EQUAL_INT(0,
        rsa_verify_pkcs1_v15_sha256(v4_hash, v4_tampered_signature, rsa_test_modulus));
}

void test_rsa_verify_v5_mismatched_hash_signature_fails(void)
{
    TEST_ASSERT_EQUAL_INT(0,
        rsa_verify_pkcs1_v15_sha256(v5_wrong_hash, v5_mismatched_signature, rsa_test_modulus));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rsa_verify_v1_hello_world);
    RUN_TEST(test_rsa_verify_v2_64kb_image);
    RUN_TEST(test_rsa_verify_v3_empty_input);
    RUN_TEST(test_rsa_verify_v4_tampered_signature_fails);
    RUN_TEST(test_rsa_verify_v5_mismatched_hash_signature_fails);
    return UNITY_END();
}
