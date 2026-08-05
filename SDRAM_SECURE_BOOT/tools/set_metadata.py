#!/usr/bin/env python3
"""
Secure metadata sector binary 생성 — magic + seq + min_ver + RSA-2048 서명.

서명: SHA-256(header) → PKCS#1 v1.5 + RSA-2048 with private key from
    tests/vectors/rsa_test_key.pem.

Usage:
    python3 tools/set_metadata.py --seq 5 --min-version 1
    ./flash_mcu.sh metadata
"""


import os
import argparse
import hashlib
import struct
import sys
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

PROJ = Path(__file__).parent.parent
KEY_DIR = Path(os.environ.get("SB_KEY_DIR", Path.home() / ".secure_boot_keys"))
KEY_PATH = KEY_DIR / "root_private.pem"

SECTOR_SIZE=0x1000
HEADER_SIZE=0x60
SIG_OFFSET=0x60
SIG_SIZE=256
MAGIC=0x5EC8B007

DIGEST_A_OFFSET = 0x10          # metadata 안의 App A 측정값 위치
DIGEST_B_OFFSET = 0x30          # metadata 안의 App B 측정값 위치
IMG_SIZE_OFFSET = 0x20          # image header 안의 code 길이 필드 위치

def app_measurement(bin_path: Path) -> bytes:
    """App binary의 SHA-256 해시 계산"""

    data=bin_path.read_bytes()
    if len(data)<IMG_SIZE_OFFSET+4:
        sys.exit(f"{bin_path}: 이미지가 너무 작습니다")

    size=struct.unpack_from("<I", data, IMG_SIZE_OFFSET)[0]
    if size==0 or size + SIG_SIZE > len(data):
        sys.exit(f"{bin_path}: header 의 size({size})가 파일 크기({len(data)})와 맞지 않습니다. "
                   f"서명 전 이미지를 가리키고 있지 않은지 확인해 주세요")

    return hashlib.sha256(data[:size]).digest()

def make_metadata(seq: int, min_version: int, key_path: Path,
                digest_a: bytes, digest_b: bytes) -> bytes:
    buf = bytearray(b"\xFF" * SECTOR_SIZE)

    struct.pack_into("<I", buf, 0x00, MAGIC)
    struct.pack_into("<I", buf, 0x04, seq)
    struct.pack_into("<I", buf, 0x08, min_version)
    struct.pack_into("<I", buf, 0x0C, 0)                 # reserved0

    # 정책이 승인하는 App 의 신원(측정값). 서명이 이 값까지 덮으므로
    # 다른 앱을 끼우면 Stage 2 의 측정값 대조에서 걸립니다.
    buf[DIGEST_A_OFFSET:DIGEST_A_OFFSET + 32] = digest_a
    buf[DIGEST_B_OFFSET:DIGEST_B_OFFSET + 32] = digest_b
    buf[0x50:0x60] = b"\x00" * 16                        # reserved1

    with key_path.open("rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    signature = key.sign(bytes(buf[:HEADER_SIZE]),
                        padding.PKCS1v15(), hashes.SHA256())
    assert len(signature) == SIG_SIZE

    buf[SIG_OFFSET:SIG_OFFSET + SIG_SIZE] = signature

    return bytes(buf)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seq", type=int, required=True)
    ap.add_argument("--min-version", type=int, required=True)
    ap.add_argument("--app-a", default="build/app/app_a/app_a.bin",
                    help="정책이 승인할 App A 이미지 (측정값 계산 대상)")
    ap.add_argument("--app-b", default="build/app/app_b/app_b.bin",
                    help="정책이 승인할 App B 이미지 (측정값 계산 대상)")
    ap.add_argument("--key", default=str(KEY_PATH))
    ap.add_argument("--out", default="build/metadata.bin")
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    digest_a = app_measurement(Path(args.app_a))
    digest_b = app_measurement(Path(args.app_b))
    data = make_metadata(args.seq, args.min_version, Path(args.key), digest_a, digest_b)
    out.write_bytes(data)

    print(f"Wrote {out}")
    print(f"  magic       = 0x{MAGIC:08X}")
    print(f"  seq         = {args.seq}")
    print(f"  min_version = {args.min_version}")
    print(f"  App A 측정값 = {digest_a.hex()}")
    print(f"  App B 측정값 = {digest_b.hex()}")
    print(f"  SHA-256(header) = {hashlib.sha256(data[:HEADER_SIZE]).hexdigest()}")
    print(f"  signature       = {data[SIG_OFFSET:SIG_OFFSET+16].hex()}... ({SIG_SIZE} B)")
    print(f"  flash primary: pyocd flash {out} --base-address 0x600C8000")
    print(f"  flash backup : pyocd flash {out} --base-address 0x600C9000")

if __name__ == "__main__":
    main()

    