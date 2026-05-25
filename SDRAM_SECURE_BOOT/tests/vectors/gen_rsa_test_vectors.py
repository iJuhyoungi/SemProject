#!/usr/bin/env python3
"""
RSA-2048 PKCS#1 v1.5 SHA-256 test vector generator.

cryptography 라이브러리로 keypair + signature 생성.
재현 가능하게 fixed seed RSA 키를 만들면 좋은데, cryptography 는
seedable 키 생성 API 가 없어 생성된 키를 PEM 으로 저장 후 재사용.

처음 실행 시 키 생성 + 저장. 재실행 시 기존 키 재사용.
"""
import hashlib
import os
import sys
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa

VECTORS_DIR = Path(__file__).parent
KEY_PATH = VECTORS_DIR / "rsa_test_key.pem"
BN_WORDS = 64
RSA_BYTES = 256

# ────────────────────────────────────────────────
# 1) Get or generate RSA-2048 keypair
# ────────────────────────────────────────────────
if KEY_PATH.exists():
    with KEY_PATH.open("rb") as f:
        private_key = serialization.load_pem_private_key(f.read(), password=None)
else:
    print(f"# Generating new RSA-2048 keypair → {KEY_PATH.name}", file=sys.stderr)
    private_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    with KEY_PATH.open("wb") as f:
        f.write(private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.NoEncryption(),
        ))

public_key = private_key.public_key()
n = public_key.public_numbers().n   # 2048-bit modulus integer


# ────────────────────────────────────────────────
# 2) Generate test signatures
# ────────────────────────────────────────────────
def sign(message: bytes) -> bytes:
    return private_key.sign(message, padding.PKCS1v15(), hashes.SHA256())


def to_le_words(num: int, n_words: int) -> list[int]:
    words = []
    for _ in range(n_words):
        words.append(num & 0xFFFFFFFF)
        num >>= 32
    return words


def emit_words(name: str, words: list[int]):
    print(f"    static const uint32_t {name}[{len(words)}] = {{")
    for i in range(0, len(words), 4):
        chunk = ", ".join(f"0x{w:08X}u" for w in words[i:i + 4])
        print(f"        {chunk},")
    print("    };")


def emit_bytes(name: str, data: bytes):
    print(f"    static const uint8_t {name}[{len(data)}] = {{")
    for i in range(0, len(data), 8):
        chunk = ", ".join(f"0x{b:02x}u" for b in data[i:i + 8])
        print(f"        {chunk},")
    print("    };")


# ────────────────────────────────────────────────
# 3) Output C header
# ────────────────────────────────────────────────
print("#ifndef RSA_TEST_VECTORS_H")
print("#define RSA_TEST_VECTORS_H")
print("#include <stdint.h>")
print()

# Common modulus 
print("/* === Public modulus (shared by all vectors) === */")
emit_words("rsa_test_modulus", to_le_words(n, BN_WORDS))
print()

# Vector 1: simple "hello world"
msg1 = b"hello world"
hash1 = hashlib.sha256(msg1).digest()
sig1 = sign(msg1)
print("/* === V1: simple 'hello world' === */")
emit_bytes("v1_hash", hash1)
emit_bytes("v1_signature", sig1)
print()

# Vector 2: 64KB image (typical Stage 2)
msg2 = bytes((i * 31) & 0xFF for i in range(64 * 1024))
hash2 = hashlib.sha256(msg2).digest()
sig2 = sign(msg2)
print("/* === V2: 64KB pseudo-image === */")
emit_bytes("v2_hash", hash2)
emit_bytes("v2_signature", sig2)
print() 

# Vector 3: empty input
msg3 = b""
hash3 = hashlib.sha256(msg3).digest()
sig3 = sign(msg3)
print("/* === V3: empty input === */")
emit_bytes("v3_hash", hash3)
emit_bytes("v3_signature", sig3)
print()

# Vector 4: tampered signature (last byte flipped) — verify must FAIL
sig4 = bytearray(sig1)
sig4[-1] ^= 0x01
print("/* === V4: tampered signature (V1's sig with last byte flipped, expect FAIL) === */")
emit_bytes("v4_hash", hash1)
emit_bytes("v4_tampered_signature", bytes(sig4))
print()

# Vector 5: wrong hash (V2's signature with V1's hash) — verify must FAIL
print("/* === V5: V2's signature with V1's hash (expect FAIL) === */")
emit_bytes("v5_wrong_hash", hash1)
emit_bytes("v5_mismatched_signature", sig2)
print()

print("#endif /* RSA_TEST_VECTORS_H */")