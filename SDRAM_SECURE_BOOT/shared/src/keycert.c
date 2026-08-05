#include "keycert.h"
#include "sha256.h"
#include "rsa.h"
#include "secure.h"
#include "embedded_pubkey.h"

#define KEYCERT_STEP_COUNT 2u

sec_bool_t keycert_load(bn_t out_modulus,
                        uint32_t *out_key_id,
                        uint32_t *out_key_version,
                        keycert_reason_t *reason)
{
    /* 기본값은 언제나 실패입니다. */
    sec_bool_t result = SEC_FAIL;
    volatile uint32_t steps = 0u;

    keycert_reason_t local_reason = KEYCERT_BAD_MAGIC;

    const keycert_t *cert = (const keycert_t *)KEYCERT_BASE;

    /* 1. magic */
    if (cert->magic != KEYCERT_MAGIC)
    {
        local_reason = KEYCERT_BAD_MAGIC;
        goto done;
    }
    ++steps;

    /* 2. root 서명 검증 — 신뢰 기점은 Stage 코드에 박힌 root modulus 입니다 */
    {
        uint8_t hash[SHA256_DIGEST_SIZE];
        SHA256_Compute((const uint8_t *)cert, KEYCERT_HEADER_SIZE, hash);

        if (!SEC_IS_PASS(rsa_verify_pkcs1_v15_sha256(hash, cert->signature,
                                                    EMBEDDED_ROOT_MODULUS)))
        {
            local_reason = KEYCERT_BAD_SIGNATURE;
            goto done;
        }
    }
    ++steps;

    if (steps != KEYCERT_STEP_COUNT)
    {
        local_reason = KEYCERT_BAD_SIGNATURE;
        goto done;
    }

    /* 검증을 통과한 뒤에만 modulus 를 꺼냅니다. 검증 전에 꺼내두면 실패
    * 경로에서 그 값이 쓰일 여지가 생깁니다. */
    bn_from_bytes_be(out_modulus, cert->modulus_be);

    if (out_key_id)      *out_key_id = cert->key_id;
    if (out_key_version) *out_key_version = cert->key_version;

    local_reason = KEYCERT_OK;
    result = SEC_PASS;

done:
    if (reason) *reason = local_reason;
    return result;
}
