#include "metadata.h"
#include "sha256.h"
#include "rsa.h"
#include "embedded_pubkey.h"
#include <string.h>

static metadata_reason_t validate(const metadata_t *m)
{
    if (m->magic != METADATA_MAGIC)
    {
        return METADATA_BAD_MAGIC;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    SHA256_Compute((const uint8_t*)m, METADATA_HEADER_SIZE, hash);
    
    /* RSA verify */
    if(!rsa_verify_pkcs1_v15_sha256(hash,m->signature,EMBEDDED_PUBKEY_MODULUS)){
        return METADATA_BAD_SIGNATURE;
    }

    return METADATA_OK;
}

int metadata_read_active(metadata_t *out, metadata_reason_t *primary_reason, metadata_reason_t *backup_reason)
{
    const metadata_t *primary = (const metadata_t *)METADATA_PRIMARY_BASE;
    const metadata_t *backup = (const metadata_t *)METADATA_BACKUP_BASE;

    metadata_reason_t pr=validate(primary);
    metadata_reason_t br=validate(backup);

    if(primary_reason) *primary_reason=pr;
    if(backup_reason)*backup_reason=br;


    int p_ok = (pr==METADATA_OK);
    int b_ok = (br==METADATA_OK);

    if (p_ok && b_ok)
    {
        const metadata_t *src = (primary->sequence_number >= backup->sequence_number) ? primary : backup;
        *out = *src;
        return 1;
    }

    if (p_ok)
    {
        *out = *primary;
        return 1;
    }
    if (b_ok)
    {
        *out = *backup;
        return 1;
    }

    return 0;
}
