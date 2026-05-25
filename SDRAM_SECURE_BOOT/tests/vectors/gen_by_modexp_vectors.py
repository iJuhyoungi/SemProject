#!/usr/bin/python3
"""bn_modexp reference : m^e mod n"""

import random

BN_WORDS=64

def to_le_words(num: int, n_words: int) -> list[int]:
    words=[]
    for _ in range(n_words):
        words.append(num&0xFFFFFFFF)
        num>>=32
    return words

def emit_c_array(name: str, words: list[int]) -> str:
    lines=[f"static const uint32_t {name}[{len(words)}] = {{"]
    for i in range(0,len(words),4):
        chunk=", ".join(f"0x{w:08x}u" for w in words[i:i+4])
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)

def emit_vector(label: str, m: int, e: int, n: int):
    r=pow(m,e,n)
    print(f"/* === {label} === m^e mod n where e=0x{e:X} */")
    print(emit_c_array(f"{label}_m", to_le_words(m, BN_WORDS)))
    print(emit_c_array(f"{label}_n", to_le_words(n, BN_WORDS)))
    print(f"    static const uint32_t {label}_e = 0x{e:X}u;")
    print(emit_c_array(f"{label}_r", to_le_words(r, BN_WORDS)))
    print()
    
if __name__=="__main__":
    print("#ifndef BN_MODEXP_VECTORS_H")
    print("#define BN_MODEXP_VECTORS_H")
    print("#include <stdint.h>")
    print()

    random.seed(0xCAFEBABE)

    emit_vector("v1_small",        m=2,     e=10,    n=1000)        # 2^10=1024, mod 1000=24
    emit_vector("v2_e_one",        m=12345, e=1,     n=99991)       # m^1=m
    emit_vector("v3_e_zero",       m=12345, e=0,     n=99991)       # m^0=1
    emit_vector("v4_e_65537_small",m=3,     e=65537, n=999983)      # RSA-e on small numbers

    # V5: RSA-2048 실제 크기 — m, n 모두 2048비트, e=65537
    n5 = (1 << 2047) | random.getrandbits(2047) | 1
    m5 = random.getrandbits(2048) % n5
    emit_vector("v5_rsa_2048",     m5, 65537, n5)

    print("#endif")