#!/usr/bin/env python3

import argparse
import hashlib
import os
import struct
import sys
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

KEY_DIR = Path(os.environ.get("SB_KEY_DIR", Path.home() / ".secure_boot_keys"))
ROOT_KEY = KEY_DIR / "root_private.pem"
RELEASE_KEY = KEY_DIR / "release_v1_private.pem"

SECTOR_SIZE = 0x1000
MAGIC = 0x4B435254          # "KCRT"
HEADER_SIZE = 0x200         # root 서명이 덮는 범위
MODULUS_OFFSET = 0x010
MODULUS_SIZE = 256
SIG_OFFSET = 0x200
SIG_SIZE = 256


def load_private(path: Path):
    if not path.exists():
        sys.exit(f"키가 없습니다: {path}\n  openssl genrsa -out {path} 2048")
    with path.open("rb") as f:
        return serialization.load_pem_private_key(f.read(), password=None)


def make_cert(key_id: int, key_version: int,
            root_key_path: Path, release_key_path: Path) -> bytes:
    release = load_private(release_key_path)
    root = load_private(root_key_path)

    n = release.public_key().public_numbers().n
    modulus_be = n.to_bytes(MODULUS_SIZE, "big")

    buf = bytearray(b"\xFF" * SECTOR_SIZE)
    struct.pack_into("<I", buf, 0x000, MAGIC)
    struct.pack_into("<I", buf, 0x004, key_id)
    struct.pack_into("<I", buf, 0x008, key_version)
    struct.pack_into("<I", buf, 0x00C, 0)

    buf[MODULUS_OFFSET:MODULUS_OFFSET + MODULUS_SIZE] = modulus_be
    buf[0x110:HEADER_SIZE] = b"\x00" * (HEADER_SIZE - 0x110)

    signature = root.sign(bytes(buf[:HEADER_SIZE]),
                        padding.PKCS1v15(), hashes.SHA256())
    assert len(signature) == SIG_SIZE
    buf[SIG_OFFSET:SIG_OFFSET + SIG_SIZE] = signature

    return bytes(buf)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--key-id", type=int, default=1)
    ap.add_argument("--key-version", type=int, default=1,
                    help="폐기 기준이 되는 버전 (H-3c 에서 사용)")
    ap.add_argument("--root-key", default=str(ROOT_KEY))
    ap.add_argument("--release-key", default=str(RELEASE_KEY))
    ap.add_argument("--out", default="build/keycert.bin")
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    data = make_cert(args.key_id, args.key_version,
                    Path(args.root_key), Path(args.release_key))
    out.write_bytes(data)

    print(f"Wrote {out}")
    print(f"  magic        = 0x{MAGIC:08X} (\"KCRT\")")
    print(f"  key_id       = {args.key_id}")
    print(f"  key_version  = {args.key_version}")
    print(f"  release key  = {args.release_key}")
    print(f"  modulus(BE)  = {data[MODULUS_OFFSET:MODULUS_OFFSET+16].hex()}... ({MODULUS_SIZE} B)")
    print(f"  SHA-256(header) = {hashlib.sha256(data[:HEADER_SIZE]).hexdigest()}")
    print(f"  root signature  = {data[SIG_OFFSET:SIG_OFFSET+16].hex()}... ({SIG_SIZE} B)")
    print(f"  flash: pyocd flash {out} --base-address 0x600CA000")


if __name__ == "__main__":
    main()