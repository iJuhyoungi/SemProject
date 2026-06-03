#!/usr/bin/env python3
"""
Rollback metadata sector (0x600C8000) 의 첫 4바이트에 min_acceptable_version 값을 씀

Usage:
    python3 tools/set_min_version.py --version N --out build/metadata.bin
    pyocd flash build/metadata.bin --base-address 0x600C8000 -t mimxrt1020
"""

import argparse
import struct
import sys
from pathlib import Path

SECTOR_SIZE=0x1000          # 4KB

def make_metadate(version: int) -> bytes:
    buf=bytearray(b"\xFF" * SECTOR_SIZE)    # erase state로 채움
    struct.pack_into("<I", buf, 0x00, version)
    return bytes(buf)

def main():
    ap=argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("--version", type=int, required=True, help="minimum acceptable version (uint32)")
    ap.add_argument("--out", default="build/metadata.bin", help="output binary path (default: build/metadata.bin)")
    args=ap.parse_args()
    
    out=Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(make_metadate(args.version))
    print(f"Wrote {out} with min_acceptable_version={args.version} (0x{args.version:08x}) @ offset 0x00")
    print(f"  flash with: pyocd flash {out} --base-address 0x600C8000 -t mimxrt1020")
    
if __name__ == "__main__":
    main()
    