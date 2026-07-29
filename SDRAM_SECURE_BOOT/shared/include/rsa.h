#ifndef RSA_H
#define RSA_H

#include <stdint.h>
#include "bignum.h"
#include "sha256.h"
#include "secure.h"

#define RSA_MODULUS_BYTES       BN_BYTES
#define RSA_PUBLIC_EXPONENT     65537u      /* 0x10001, 일반적으로 RSA 공개 지수로 사용되는 작은 값 */

/* RSA-2048 PKCS#1 v1.5 with SHA-256 signature verification.
*
* @param expected_hash:  SHA-256 of the data being verified (32 bytes)
* @param signature:      256-byte RSA signature (BE bytes, RFC 8017 형식)
* @param modulus:        public key modulus (bn_t, 2048-bit)
* @return SEC_PASS if verification PASS, SEC_FAIL otherwise.
*         반드시 SEC_IS_PASS() 로 확인해야 합니다. 0 이 아니라는 것만 보면
*         SEC_FAIL 도 참이 되므로 판정이 뒤집힙니다.
*
* 공개 지수는 65537 hard-coded (RSA 산업 표준).
*/

sec_bool_t rsa_verify_pkcs1_v15_sha256(
    const uint8_t expected_hash[SHA256_DIGEST_SIZE],
    const uint8_t signature[RSA_MODULUS_BYTES],
    const bn_t modulus
);

#endif /* RSA_H */