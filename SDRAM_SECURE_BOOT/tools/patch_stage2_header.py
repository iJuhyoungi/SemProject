#!/usr/bin/env python3
"""
Patch Stage 2 image header (vector-table reserved slots).

Stage 1 의 Stage2_Verify() 가 magic/size/CRC32 체크를 통과하도록
헤더 3개 슬롯을 채워넣고 in-place 저장.

스펙 (Stage 1 bootloader/stage1/src/main.c 의 IMG_* defines 와 일치):
    offset 0x1C : magic = 0xDEADBEEF   (LE uint32)
    offset 0x20 : size  = len(image)    (LE uint32)
    offset 0x24 : CRC32 = CRC32(image with bytes 0x24..0x27 treated as zero)
                          poly 0xEDB88320, init/xor-out 0xFFFFFFFF
                          (표준 IEEE 802.3 reflected — Python zlib.crc32 동일)

추가 출력으로 SHA-256 도 계산해서 보여줌 — Stage 1 의
'[Verify] SHA-256:' UART 출력과 비교할 정답값.

Step 3 에서 SHA-256 + RSA 서명 첨부 도구로 확장 예정.
"""
import argparse
import hashlib
import struct
import sys
import zlib
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding

MAGIC = 0xDEADBEEF
OFF_MAGIC = 0x1C
OFF_SIZE = 0x20
OFF_CRC = 0x24
DEFAULT_KEY = Path(__file__).parent.parent / "tests/vectors/rsa_test_key.pem"


def patch(path: str, key_path: Path = DEFAULT_KEY) -> None:
    with open(path, "rb") as f:
        buf = bytearray(f.read())

    img_size = len(buf)
    if img_size < OFF_CRC + 4:
        sys.exit(f"image too small ({img_size} bytes) — header slots not reachable")

    # 1. magic + size
    struct.pack_into("<I", buf, OFF_MAGIC, MAGIC)
    struct.pack_into("<I", buf, OFF_SIZE, img_size)

    # 2. CRC32 — CRC 필드 자체는 0 으로 두고 계산
    #    (C 측 CRC32_ComputeWithSkip 가 이 영역을 0 으로 본 채 계산하는 것과 일치)
    buf[OFF_CRC:OFF_CRC + 4] = b"\x00\x00\x00\x00"
    crc = zlib.crc32(bytes(buf)) & 0xFFFFFFFF
    struct.pack_into("<I", buf, OFF_CRC, crc)

    # 3. SHA-256 — Stage 1 의 verify chain 이 출력해야 할 예상값
    #    Stage 1 은 [base, base+size) 전체를 hash 하므로, CRC 필드까지 들어간
    #    최종 image 의 sha256 이 그대로 보드 출력과 일치해야 한다.
    sha = hashlib.sha256(bytes(buf)).hexdigest()
    
    # private key로 서명
    with open(key_path, "rb") as f:
        key=serialization.load_pem_private_key(f.read(), password=None)
    signature=key.sign(bytes(buf), padding.PKCS1v15(),hashes.SHA256())
    assert len(signature)==256, "RSA-2048 signature must be 256 bytes"

    with open(path, "wb") as f:
        f.write(buf)
        f.write(signature)

    print(f"Patched: {path}")
    print(f"  size   = {img_size} (0x{img_size:08x}) @ offset 0x{OFF_SIZE:02x}")
    print(f"  magic  = 0x{MAGIC:08x} @ offset 0x{OFF_MAGIC:02x}")
    print(f"  crc32  = 0x{crc:08x} @ offset 0x{OFF_CRC:02x}")
    print(f"  sha256 = {sha}")
    print(f"  signature = {signature[:16].hex()}... ({len(signature)} bytes) @ offset 0x{img_size:x}")
    print("           ^ expected '[Verify] SHA-256:' line on UART")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("image", help="path to stage2.bin (patched in-place)")
    ap.add_argument("--key", default=str(DEFAULT_KEY), help="RSA private key PEM")
    args = ap.parse_args()
    patch(args.image, Path(args.key))


if __name__ == "__main__":
    main()
