#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>
#include <stddef.h>

#define BN_BITS     2048u
#define BN_WORDS    (BN_BITS / 32u) /* 2048/32 = 64 단어, 256바이트 */
#define BN_BYTES    (BN_BITS / 8u)  /* 2048/8 = 256 바이트 */

/*
* Little-endian word ordering: v[0] = LSW, v[BN_WORDS-1] = MSW.
* RSA-2048 외 다른 크기는 안 다룸 — 단일 용도라 고정 크기로 단순화.
*/
typedef uint32_t bn_t[BN_WORDS];

void bn_zero(bn_t r);                           /* r = 0 */
void bn_copy(bn_t r, const bn_t a);             /* r = a */ 
int bn_is_zero(const bn_t a);                   /* 1 if a == 0 else 0 */
int bn_cmp(const bn_t a, const bn_t b);         /* -1 / 0 / +1  (a<b / a==b / a>b) */

/* 산술 - 반환값 = 최상위 워드 밖으로 빠져나간 carry/borrow(0 or 1) */
uint32_t bn_add(bn_t r, const bn_t a, const bn_t b);        // r = a + b
uint32_t bn_sub(bn_t r, const bn_t a, const bn_t b);        // r = a - b

void bn_from_bytes_be(bn_t r, const uint8_t bytes[BN_BYTES]);
void bn_to_bytes_be(uint8_t bytes[BN_BYTES], const bn_t a);

/* multiplication 결과용 - 4096bit */
#define BN_WORDS_2X (BN_WORDS * 2u)
typedef uint32_t bn2_t[BN_WORDS_2X];

void bn2_zero(bn2_t r);
void bn_mul(bn2_t prod, const bn_t a, const bn_t b);        // prod = a * b
/**
 * modular reduction : r = a mod n
 * 입력 a는 128워드,
 * 결과 r은 64워드(n보다는 작은 값),
 * n!=0을 가정(RSA modules은 항상 nonzero)
 */
void bn_mod(bn_t r, const bn2_t a, const bn_t n);          // r = a mod n  (a는 4096비트, n과 r은 2048비트)

#endif /* BIGNUM_H */
