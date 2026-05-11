#!/usr/bin/env python3
"""
UART uploader for SDRAM_OTA_AB recovery mode.

Sends a new Stage 2 binary to the board over UART, following the
5-phase protocol defined in docs/PROTOCOL_RECOVERY_UART.md.
"""

import argparse
import sys
import zlib
import serial

MAX_STAGE2_SIZE=0x3C000

def load_binary(path):
    """Read binary file, return raw bytes."""
    with open(path, "rb") as f:
        return f.read()
    
def compute_crc32(data):
    """IEEE 802.3 CRC32."""
    return zlib.crc32(data) & 0xFFFFFFFF

def handshake(ser):
    """Phase 1: send RECV?\\n, expect READY!\\r\\n."""
    request = b"RECV?\n"
    expected = b"READY!\r\n"

    print(f"[uploader] phase 1: sending handshake ({request!r})...")
    ser.write(request)
    ser.flush()

    response = ser.read(len(expected))
    print(f"[uploader] phase 1: received {response!r}")

    if response != expected:
        raise RuntimeError(
            f"handshake failed: expected {expected!r}, got {response!r}"
        )
    print(f"[uploader] phase 1: handshake OK")

def parse_args():
    parser=argparse.ArgumentParser(
        description="Upload a new Stage 2 binary via UART recovery protocol."
    )
    parser.add_argument(
        "binary",
        help="Path to the Stage 2 binary file (e.g., bootloader_stage2.bin)",
    )
    parser.add_argument(
        "--port",
        default="/dev/ttyACM0",
        help="Serial port device (default: /dev/ttyACM0)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Read timeout in seconds (default: 10.0)",
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # 1. Load binary
    print(f"[uploader] loading {args.binary}...")
    data = load_binary(args.binary)
    size = len(data)
    print(f"[uploader] size: {size} bytes (0x{size:X})")

    # 2. Size validation (mirror board's STATE_RECV_SIZE check)
    if size == 0:
        print(f"[uploader] error: binary is empty", file=sys.stderr)
        return 1
    if size > MAX_STAGE2_SIZE:
        print(
            f"[uploader] error: size {size} exceeds MAX_STAGE2_SIZE {MAX_STAGE2_SIZE}",
            file=sys.stderr,
        )
        return 1

    # 3. Compute CRC32
    crc = compute_crc32(data)
    print(f"[uploader] crc32: 0x{crc:08X}")

    # 4. Open port (그대로)
    print(f"[uploader] opening {args.port} @ {args.baud}...")
    ser = serial.Serial(args.port, args.baud, timeout=args.timeout)
    print(f"[uploader] port open: {ser.name}")
    try:
        handshake(ser)
    finally:
        ser.close()
        print(f"[uploader] port closed")




if __name__ == "__main__":
    sys.exit(main())
