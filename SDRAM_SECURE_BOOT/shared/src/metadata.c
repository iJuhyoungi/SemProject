#include "metadata.h"
#include "sha256.h"
#include "rsa.h"
#include "embedded_pubkey.h"
#include <string.h>

static sec_bool_t validate(const metadata_t *m, metadata_reason_t *reason)
{
    sec_bool_t result = SEC_FAIL;

    /*진단값도 실패에서 시작*/
    *reason = METADATA_BAD_MAGIC;

    if (m->magic != METADATA_MAGIC)
    {
        *reason = METADATA_BAD_MAGIC;
        return SEC_FAIL;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t *)m, METADATA_HEADER_SIZE, hash);

    /* RSA verify */
    if (!SEC_IS_PASS(rsa_verify_pkcs1_v15_sha256(hash, m->signature, EMBEDDED_ROOT_MODULUS)))
    {
        *reason = METADATA_BAD_SIGNATURE;
        return SEC_FAIL;
    }

    *reason = METADATA_OK;
    result = SEC_PASS;

    return result;
}

sec_bool_t metadata_read_active(metadata_t *out, metadata_reason_t *primary_reason, metadata_reason_t *backup_reason)
{
    const metadata_t *primary = (const metadata_t *)METADATA_PRIMARY_BASE;
    const metadata_t *backup = (const metadata_t *)METADATA_BACKUP_BASE;

    metadata_reason_t pr = METADATA_BAD_MAGIC;
    metadata_reason_t br = METADATA_BAD_MAGIC;

    sec_bool_t p_ok=validate(primary, &pr);
    sec_bool_t b_ok=validate(backup, &br);

    if (primary_reason)
        *primary_reason = pr;
    if (backup_reason)
        *backup_reason = br;

    if (SEC_IS_PASS(p_ok) && SEC_IS_PASS(b_ok))
    {
        const metadata_t *src = (primary->sequence_number >= backup->sequence_number) ? primary : backup;
        *out = *src;
        return SEC_PASS;
    }

    if (SEC_IS_PASS(p_ok))
    {
        *out = *primary;
        return SEC_PASS;
    }
    if (SEC_IS_PASS(b_ok))
    {
        *out = *backup;
        return SEC_PASS;
    }

    return SEC_FAIL;
}
