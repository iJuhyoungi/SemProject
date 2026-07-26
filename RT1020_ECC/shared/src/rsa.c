#include "rsa.h"
#include <string.h>

/* SHA-256 의 DER-encoded DigestInfo prefix (RFC 8017 Appendix A.2.4) */
static const uint8_t SHA256_DIGEST_INFO_PREFIX[19] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

#define DIGEST_INFO_LEN (sizeof(SHA256_DIGEST_INFO_PREFIX) + SHA256_DIGEST_SIZE)

int rsa_verify_pkcs1_v15_sha256(
    const uint8_t expected_hash[SHA256_DIGEST_SIZE],
    const uint8_t signature[RSA_MODULUS_BYTES],
    const bn_t modulus)
{
    /*signature^e mod n → decoded[256]*/
    bn_t sig_bn;
    bn_from_bytes_be(sig_bn, signature);

    bn_t decoded_bn;
    bn_modexp(decoded_bn, sig_bn, RSA_PUBLIC_EXPONENT, modulus);

    uint8_t decoded[RSA_MODULUS_BYTES];
    bn_to_bytes_be(decoded, decoded_bn);

    /* magic bytes 검증*/
    if (decoded[0] != 0x00 || decoded[1] != 0x01)
    {
        return 0; /* invalid signature */
    }

    /*PS — K - 3 - len(T) = 256 -3 - 51 = 202 bytes of 0xFF*/
    const uint32_t ps_start = 2;
    const uint32_t ps_end_exclusive = RSA_MODULUS_BYTES - DIGEST_INFO_LEN - 1; // 204

    /* PS 0xFF 검증 : PKCS#1 v1.5 표준으로는 PS길이가 최소 8 bytes 이상 */
    if (ps_end_exclusive - ps_start < 8)
        return 0; /* invalid signature */

    for (uint32_t i = ps_start; i < ps_end_exclusive; ++i)
    {        if (decoded[i] != 0xFF)
            return 0; /* invalid signature */
    }

    /*PS와 DigestInfo의 separator*/
    if (decoded[ps_end_exclusive] != 0x00)
        return 0; /* invalid signature */

    /*DigestInfo prefix 검증*/
    const uint32_t prefix_start = ps_end_exclusive + 1;
    if (memcmp(&decoded[prefix_start],
               SHA256_DIGEST_INFO_PREFIX,
               sizeof(SHA256_DIGEST_INFO_PREFIX)) != 0)
    {
        return 0; /* invalid signature */
    }

    /*hash 비교*/
    const uint32_t hash_start = RSA_MODULUS_BYTES - SHA256_DIGEST_SIZE; // 256-32=224
    if (memcmp(&decoded[hash_start], expected_hash, SHA256_DIGEST_SIZE) != 0)
        return 0; /* invalid signature */

    return 1; /* signature is valid */
}
