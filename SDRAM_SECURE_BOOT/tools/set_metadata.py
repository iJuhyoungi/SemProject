#!/usr/bin/env python3
"""
Secure metadata sector binary 생성 — magic + seq + min_ver + SHA-256.

Usage:
    python3 tools/set_metadata.py --seq 5 --min-version 1 \
        --out build/metadata.bin
    pyocd flash build/metadata.bin --base-address 0x600C8000 -t mimxrt1020
    # 또는 backup 영역:
    pyocd flash build/metadata.bin --base-address 0x600C9000 -t mimxrt1020

Dual metadata 운용: 큰 seq 가 active. update 시 다른 slot 에 (현재 active 의 seq + 1)
새 metadata 쓰면 다음 부팅 시 자동 활성.
"""

import argparse
import hashlib
import struct
import sys
from pathlib import Path

SECTOR_SIZE=0x1000
HEADER_SIZE=0x20
MAGIC=0x5EC8B007

def make_metadata(seq: int, min_version: int) -> bytes:
    buf=bytearray(b"\xFF"*SECTOR_SIZE)
    
    struct.pack_into("<I",buf,0x00,MAGIC)
    struct.pack_into("<I",buf,0x04,seq)
    struct.pack_into("<I",buf,0x08,min_version)
    
    # SHA-256 해시 계산 (metadata header + padding)
    sha=hashlib.sha256(bytes(buf[:HEADER_SIZE])).digest()
    buf[HEADER_SIZE:HEADER_SIZE+32]=sha
    
    return bytes(buf)

def main():
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--seq", type=int, required=True,
        help="sequence_number (uint32). 큰 값이 active")
    ap.add_argument("--min-version", type=int, required=True,
        help="min_acceptable_version (uint32)")
    ap.add_argument("--out", default="build/metadata.bin",
        help="output binary path")
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(make_metadata(args.seq, args.min_version))

    print(f"Wrote {out}")
    print(f"  magic       = 0x{MAGIC:08X}")
    print(f"  seq         = {args.seq}")
    print(f"  min_version = {args.min_version}")
    print(f"  SHA-256     = {hashlib.sha256(bytes(make_metadata(args.seq, args.min_version)[:HEADER_SIZE])).hexdigest()}")
    print(f"  flash primary: pyocd flash {out} --base-address 0x600C8000")
    print(f"  flash backup : pyocd flash {out} --base-address 0x600C9000")

if __name__ == "__main__":
    main()
    