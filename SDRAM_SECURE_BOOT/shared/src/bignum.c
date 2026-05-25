#include "bignum.h"

void bn_zero(bn_t r)
{
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        r[i] = 0u;
    }
}

void bn_copy(bn_t r, const bn_t a)
{
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        r[i] = a[i];
    }
}

int bn_is_zero(const bn_t a)
{
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        if (a[i] != 0u)
        {
            return 0;
        }
    }
    return 1;
}

int bn_cmp(const bn_t a, const bn_t b)
{
    for (int32_t i = BN_WORDS - 1; i >= 0; --i)
    {
        if (a[i] < b[i])
        {
            return -1;
        }
        else if (a[i] > b[i])
        {
            return 1;
        }
    }
    return 0;
}

uint32_t bn_add(bn_t r, const bn_t a, const bn_t b)
{
    uint64_t sum;
    uint32_t carry = 0;
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        sum = (uint64_t)a[i] + b[i] + carry;
        r[i] = (uint32_t)sum;
        carry = (uint32_t)(sum >> 32);
    }
    return carry;
}

uint32_t bn_sub(bn_t r, const bn_t a, const bn_t b)
{
    uint64_t diff;
    uint32_t borrow = 0;
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        diff = (uint64_t)a[i] - (uint64_t)b[i] - borrow;
        r[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1u; /* 64비트 diff 가 음수면 최상위 비트가 1이므로 borrow=1, 양수면 0 */
    }
    return borrow;
}

void bn_from_bytes_be(bn_t r, const uint8_t bytes[BN_BYTES])
{
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        uint32_t off = BN_BYTES - 4u - 4u * i;
        r[i] = ((uint32_t)bytes[off + 0] << 24) | ((uint32_t)bytes[off + 1] << 16) | ((uint32_t)bytes[off + 2] << 8) | ((uint32_t)bytes[off + 3]);
    }
}

void bn_to_bytes_be(uint8_t bytes[BN_BYTES], const bn_t a)
{
    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        uint32_t off = BN_BYTES - 4u - 4u * i;

        bytes[off + 0] = (uint8_t)(a[i] >> 24);
        bytes[off + 1] = (uint8_t)((a[i] >> 16));
        bytes[off + 2] = (uint8_t)((a[i] >> 8));
        bytes[off + 3] = (uint8_t)((a[i]));
    }
}

void bn2_zero(bn2_t r)
{
    for (uint32_t i = 0; i < BN_WORDS_2X; ++i)
    {
        r[i] = 0;
    }
}

void bn_mul(bn2_t prod, const bn_t a, const bn_t b)
{
    bn2_zero(prod);

    for (uint32_t i = 0; i < BN_WORDS; ++i)
    {
        uint32_t carry = 0;
        for (uint32_t j = 0; j < BN_WORDS; ++j)
        {
            uint64_t t = (uint64_t)a[i] * b[j] + prod[i + j] + carry;
            prod[i + j] = (uint32_t)t;
            carry = (uint32_t)(t >> 32);
        }
        prod[i + BN_WORDS] = carry; // 안쪽의 루프가 종료되고 난 뒤 남은 carry는 prod[i + BN_WORDS] 에 저장 (최대 i+j = 63+63=126, carry는 127번째 워드로)
    }
}

void bn_mod(bn_t r, const bn2_t a, const bn_t n)
{
    bn_zero(r);
    uint32_t r_overflow = 0; // shift로 빠져나간 최상위 1비트

    /* a의 최상위 비트(4095)부터 0까지 한 비트씩 처리 */
    for (int i = (int)(BN_WORDS_2X * 32u) - 1; i >= 0; --i)
    {
        /* r = r << 1. 잘려나간 최상위 비트를 r_overflow로 보관 */
        uint32_t new_overflow = (r[BN_WORDS - 1] >> 31) & 1u;
        uint32_t carry = 0;
        for (int k = 0; k < BN_WORDS; ++k)
        {
            uint32_t next_carry = (r[k] >> 31) & 1u;
            r[k] = (r[k] << 1) | carry;
            carry = next_carry;
        }
        r_overflow = new_overflow; // shift로 빠져나간 최상위 비트

        /**
         * a의 i번째 비트를 r의 LSB로 shift-in 
         * r = r | (a[i] ? 1 : 0)
         * a[i]는 a의 word_idx = i/32 번째 워드의 bit_idx = i%32 번째 비트
         */
        uint32_t word_idx = (uint32_t)i >> 5u;
        uint32_t bit_idx = (uint32_t)i & 31u;
        if ((a[word_idx] >> bit_idx) & 1u)
        {
            r[0] |= 1u;
        }

        /**
         * r_total=r_overflow*2^2048 + r
         * r_total>=n이면, r -= n (r_total - n >= 0 이므로 r - n >= 0)
         * r_overflow = 1 이면, 무조건 r_total >n (n < 2^2048)
         * 아니면 bn_cmp(r, n) >= 0 인지 검사
         * 
         * 결국은 r >=n 인지 아닌지 확인
         */
        if (r_overflow > 0 || bn_cmp(r, n) >= 0)
        {
            bn_sub(r, r, n);
            r_overflow = 0; /* r에서 n을 빼면 최대값이 n-1이므로 r_overflow는 0으로 리셋 */
        }
    }
}
