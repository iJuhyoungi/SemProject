#include "metadata.h"
#include "sha256.h"
#include <string.h>

static int validate(const metadata_t *m)
{
    if (m->magic != METADATA_MAGIC)
    {
        return 0;
    }

    uint8_t computed[32];
    SHA256_Compute((const uint8_t *)m, METADATA_HEADER_SIZE, computed);
    if (memcmp(computed, m->sha256, 32) != 0)
    {
        return 0;
    }
    return 1;
}

int metadata_read_active(metadata_t *out)
{
    const metadata_t *primary = (const metadata_t *)METADATA_PRIMARY_BASE;
    const metadata_t *backup = (const metadata_t *)METADATA_BACKUP_BASE;

    int p_ok = validate(primary);
    int b_ok = validate(backup);

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
