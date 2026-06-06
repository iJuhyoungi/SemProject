#!/usr/bin/env python3
"""
Secure metadata sector binary 생성 — magic + seq + min_ver + RSA-2048 서명.

서명: SHA-256(header) → PKCS#1 v1.5 + RSA-2048 with private key from
    tests/vectors/rsa_test_key.pem.

Usage:
    python3 tools/set_metadata.py --seq 5 --min-version 1
    ./flash_mcu.sh metadata
"""


import argparse
import hashlib
import struct
import sys
from pathlib import Path

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

ROOT=Path(__file__).parent.parent
KEY_PATH=ROOT/"tests/vectors/rsa_test_key.pem"
SECTOR_SIZE=0x1000
HEADER_SIZE=0x20
SIG_OFFSET=0x20
SIG_SIZE=256
MAGIC=0x5EC8B007

def make_metadata(seq: int, min_version: int, key_path: Path) -> bytes:
    buf=bytearray(b"\xFF"*SECTOR_SIZE)
    
    struct.pack_into("<I",buf,0x00,MAGIC)
    struct.pack_into("<I",buf,0x04,seq)
    struct.pack_into("<I",buf,0x08,min_version)
    
    # RSA-2048 PKCS#1 v1.5 signature (SHA-256 해시, 256바이트) 계산하여 buf[0x20:0x120]에 저장
    with key_path.open("rb") as f:
        key=serialization.load_pem_private_key(f.read(), password=None)
    signature=key.sign(bytes(buf[:HEADER_SIZE]),
                       padding.PKCS1v15(), hashes.SHA256())
    assert len(signature)==SIG_SIZE
        
    buf[SIG_OFFSET:SIG_OFFSET+SIG_SIZE]=signature
    
    return bytes(buf)

def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seq", type=int, required=True)
    ap.add_argument("--min-version", type=int, required=True)
    ap.add_argument("--key", default=str(KEY_PATH))
    ap.add_argument("--out", default="build/metadata.bin")
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    data = make_metadata(args.seq, args.min_version, Path(args.key))
    out.write_bytes(data)

    print(f"Wrote {out}")
    print(f"  magic       = 0x{MAGIC:08X}")
    print(f"  seq         = {args.seq}")
    print(f"  min_version = {args.min_version}")
    print(f"  SHA-256(header) = {hashlib.sha256(data[:HEADER_SIZE]).hexdigest()}")
    print(f"  signature       = {data[SIG_OFFSET:SIG_OFFSET+16].hex()}... ({SIG_SIZE} B)")
    print(f"  flash primary: pyocd flash {out} --base-address 0x600C8000")
    print(f"  flash backup : pyocd flash {out} --base-address 0x600C9000")

if __name__ == "__main__":
    main()

    