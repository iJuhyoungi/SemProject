#include "rsa.h"
#include "secure.h"

#define RSA_VERIFY_STEP_COUNT   6u

/* SHA-256 의 DER-encoded DigestInfo prefix (RFC 8017 Appendix A.2.4) */
static const uint8_t SHA256_DIGEST_INFO_PREFIX[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

#define DIGEST_INFO_LEN (sizeof(SHA256_DIGEST_INFO_PREFIX) + SHA256_DIGEST_SIZE)

sec_bool_t rsa_verify_pkcs1_v15_sha256(
    const uint8_t expected_hash[SHA256_DIGEST_SIZE],
    const uint8_t signature[RSA_MODULUS_BYTES],
    const bn_t modulus)
{
    /* 기본값은 실패 */
    sec_bool_t result = SEC_FAIL;
    volatile uint32_t steps = 0u;

    /* signature^e mod n -> decoded[256] */
    bn_t sig_bn;
    bn_from_bytes_be(sig_bn, signature);

    bn_t decoded_bn;
    bn_modexp(decoded_bn, sig_bn, RSA_PUBLIC_EXPONENT, modulus);

    uint8_t decoded[RSA_MODULUS_BYTES];
    bn_to_bytes_be(decoded, decoded_bn);

    /* 1. 앞 두 바이트 = 0x00 0x01 */
    if (decoded[0] != 0x00 || decoded[1] != 0x01)
    {
        return SEC_FAIL;
    }
    ++steps;

    /* PS = K - 3 - len(T) = 256 - 3 - 51 = 202 bytes of 0xFF */
    const uint32_t ps_start = 2u;
    const uint32_t ps_end_exclusive = RSA_MODULUS_BYTES - DIGEST_INFO_LEN - 1u; /* 204 */
    const uint32_t ps_len = ps_end_exclusive - ps_start;                        /* 202 */

    /* 2. PKCS#1 v1.5 는 PS 가 최소 8 바이트여야 합니다 */
    if (ps_len < 8u)
    {
        return SEC_FAIL;
    }
    ++steps;

    /*
     * 3. PS 전 구간이 0xFF 인지 확인
     *
     * 루프가 실제로 몇 번 돌았는지를 함께 세고 끝난 뒤 compare. 
     * 글리치로 루프 카운터가 망가져 검사가 통째로 건너뛰어지면 여기서 드러납니다.
     * 대조가 없으면 패딩 검사가 사라진 채로 "통과" 가 되어 위조 서명이 열립니다.
     */
    volatile uint32_t ps_seen = 0u;
    for (uint32_t i = ps_start; i < ps_end_exclusive; ++i)
    {
        if (decoded[i] != 0xFF)
        {
            return SEC_FAIL;
        }
        ++ps_seen;
    }
    if (!SEC_IS_PASS(SEC_LOOP_CHECK(ps_seen, ps_len)))
    {
        return SEC_FAIL;
    }
    ++steps;

    /* 4. PS 와 DigestInfo 의 separator */
    if (decoded[ps_end_exclusive] != 0x00)
    {
        return SEC_FAIL;
    }
    ++steps;

    /* 5. DigestInfo prefix — 상수시간 비교 */
    const uint32_t prefix_start = ps_end_exclusive + 1u;
    if (!SEC_IS_PASS(sec_memeq(&decoded[prefix_start],
                                SHA256_DIGEST_INFO_PREFIX,
                                sizeof(SHA256_DIGEST_INFO_PREFIX))))
    {
        return SEC_FAIL;
    }
    ++steps;

    /* 6. hash 비교 — 상수시간 */
    const uint32_t hash_start = RSA_MODULUS_BYTES - SHA256_DIGEST_SIZE; /* 224 */
    if (!SEC_IS_PASS(sec_memeq(&decoded[hash_start],
                                expected_hash,
                                SHA256_DIGEST_SIZE)))
    {
        return SEC_FAIL;
    }
    ++steps;

    /* 통과한 검사 수를 compare. */
    if (steps != RSA_VERIFY_STEP_COUNT)
    {
        return SEC_FAIL;
    }

    result = SEC_PASS;
    return result;
}

