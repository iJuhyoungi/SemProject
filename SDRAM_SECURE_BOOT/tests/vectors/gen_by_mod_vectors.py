#!/usr/bin/env python3

"""
bn_mod 테스트용 reference 값 생성
Python int의 임의 정밀도와 pow()의 모듈러 지수 연산을 활용하여 정확한 결과를 얻음
"""

import random

BN_WORDS=64
BN_WORDS_2X=128
BITS=2048

def to_le_words(num: int, n_words: int) -> list[int]:
    """Python int → LE 32-bit word 배열 (v[0] = LSW)"""
    words = []
    for _ in range(n_words):
        words.append(num & 0xFFFFFFFF)
        num >>= 32
    return words 

def emit_c_array(name: str, words: list[int]) -> str:
    lines = [f"    static const uint32_t {name}[{len(words)}] = {{"]
    for i in range(0, len(words), 4):
        chunk = ", ".join(f"0x{w:08X}u" for w in words[i:i+4])
        lines.append(f"        {chunk},")
    lines.append("    };")
    return "\n".join(lines)

def emit_vector(label: str, a: int, n: int):
    r = a % n
    print(f"/* === {label} ===")
    print(f"   a (4096-bit input) % n (2048-bit modulus) = r (2048-bit) */")
    print(emit_c_array(f"{label}_a", to_le_words(a, BN_WORDS_2X)))
    print(emit_c_array(f"{label}_n", to_le_words(n, BN_WORDS)))
    print(emit_c_array(f"{label}_r", to_le_words(r, BN_WORDS)))
    print()

if __name__ == "__main__":
    random.seed(0xDEADBEEF)    # 재현 가능

    # V1: 작은 케이스 — a < n 이면 r = a
    emit_vector("v1_a_less_than_n", a=100, n=7919)

    # V2: a > n, 단순
    emit_vector("v2_simple", a=12345, n=100)

    # V3: 큰 a, 작은 n (반복 빼기 많이)
    emit_vector("v3_large_a_small_n", a=(1 << 4000), n=0xDEADBEEF)

    # V4: 비슷한 크기 (가장 흔한 RSA 시나리오)
    n4 = (1 << 2047) | random.getrandbits(2047) | 1  # 2048-bit odd
    a4 = random.getrandbits(4096)
    emit_vector("v4_rsa_size", a4, n4)

    # V5: edge — a 가 n 의 정확히 2배
    n5 = (1 << 2047) | random.getrandbits(2047) | 1
    emit_vector("v5_exactly_2n", a=2 * n5, n=n5)
    
