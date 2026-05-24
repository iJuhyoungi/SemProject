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
        prod[i + BN_WORDS] = carry;         // 안쪽의 루프가 종료되고 난 뒤 남은 carry는 prod[i + BN_WORDS] 에 저장 (최대 i+j = 63+63=126, carry는 127번째 워드로)
    }
}
