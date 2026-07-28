#ifndef SECURE_H
#define SECURE_H

#include <stdint.h>
#include <stddef.h>

#define SEC_PASS 0xA5C33C5Au
#define SEC_FAIL 0x5A3CC3A5u

typedef uint32_t sec_bool_t;

#define SEC_IS_PASS(v) (((v) == SEC_PASS) && ((v) != SEC_FAIL))

static inline sec_bool_t sec_memeq(const void *a, const void *b, size_t n)
{
    const volatile uint8_t *pa = (const volatile uint8_t *)a;
    const volatile uint8_t *pb = (const volatile uint8_t *)b;
    volatile uint8_t diff = 0;

    for (size_t i = 0; i < n; ++i)
    {
        diff |= (uint8_t)(pa[i] ^ pb[i]);
    }

    /*
    * diff 가 0 일 때만 PASS 입니다. 분기(if) 없이 마스크로 골라내서 실행 시간을
    * 입력과 무관하게 유지합니다.
    *
    *   d = 0        → (0   + 255) >> 8 = 0   → mask = 0x00000000 → PASS
    *   d = 1..255   → (d   + 255) >> 8 = 1   → mask = 0xFFFFFFFF → FAIL
    *
    * 0u - nonzero 는 0 또는 0xFFFFFFFF 를 만드는 관용구입니다. 부호 있는
    * 우측 시프트에 기대지 않아서 승격 규칙에 걸릴 여지가 없습니다.
    */
    uint32_t d = (uint32_t)diff;
    uint32_t nonzero = (d + 0xFFu) >> 8;
    uint32_t mask = 0u - nonzero;

    return (sec_bool_t)((SEC_FAIL & mask) | (SEC_PASS & ~mask));
}

#define SEC_LOOP_CHECK(counter, expected) \
    (((counter) == (expected)) ? SEC_PASS : SEC_FAIL)

#endif /* SECURE_H */
