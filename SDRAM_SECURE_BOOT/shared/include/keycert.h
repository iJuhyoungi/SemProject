#ifndef KEYCERT_H
#define KEYCERT_H

#include <stdint.h>
#include "bignum.h"
#include "secure.h"

#define KEYCERT_BASE         0x600CA000u
#define KEYCERT_MAGIC        0x4B435254u   /* "KCRT" */
#define KEYCERT_HEADER_SIZE  0x200u        /* root 서명이 덮는 범위 */
#define KEYCERT_SIG_SIZE     256u
#define KEYCERT_MODULUS_SIZE 256u

typedef struct
{
    uint32_t magic;                                 // 0x000
    uint32_t key_id;                                // 0x004
    uint32_t key_version;                           // 0x008  H-3c 폐기 기준
    uint32_t reserved0;                             // 0x00C
    uint8_t  modulus_be[KEYCERT_MODULUS_SIZE];      // 0x010 ~ 0x10F
    uint8_t  reserved1[240];                        // 0x110 ~ 0x1FF
    uint8_t  signature[KEYCERT_SIG_SIZE];           // 0x200 ~ 0x2FF (root 서명)
} keycert_t;

typedef enum
{
    KEYCERT_OK = 0,
    KEYCERT_BAD_MAGIC = 1,
    KEYCERT_BAD_SIGNATURE = 2,
} keycert_reason_t;

sec_bool_t keycert_load(bn_t out_modulus,
                        uint32_t *out_key_id,
                        uint32_t *out_key_version,
                        keycert_reason_t *reason);

#endif /* KEYCERT_H */