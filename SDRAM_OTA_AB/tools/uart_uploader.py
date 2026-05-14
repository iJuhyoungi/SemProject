#!/usr/bin/env python3
"""
UART uploader for SDRAM_OTA_AB recovery mode.

Sends a new Stage 2 binary to the board over UART, following the
5-phase protocol defined in docs/PROTOCOL_RECOVERY_UART.md.
"""

import argparse
import sys
import time
import zlib
import struct
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

    time.sleep(2.0)
    
    in_buf = ser.in_waiting
    print(f"[uploader] DEBUG: pre-handshake in_waiting = {in_buf} bytes")
    if in_buf > 0:
        peeked = ser.read(in_buf)
        print(f"[uploader] DEBUG: pre-handshake bytes = {peeked!r}")
    
    ser.reset_input_buffer()
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

def send_size(ser, size):
    """Phase 2: send size(4B little-endian), expect ACK\\r\\n. or TOOBIG!\\r\\n."""
    payload=struct.pack('<I',size)

    print(f"[uploader] phase 2: sending size ({size} bytes, 0x{size:X})...")
    ser.write(payload)
    ser.flush()

    response = ser.read_until(b'\n', size=64)
    print(f"[uploader] phase 2: received {response!r}")

    if response == b"ACK!\r\n":
        print(f"[uploader] phase 2: size accepted")
        return
    if response == b"TOOBIG!\r\n":
        raise RuntimeError(f"board rejected size {size}: TOOBIG")
    raise RuntimeError(f"phase 2 unexpected response: {response!r}")

def send_data(ser, data):
    """"Phase 3: send raw binary data (no response from board)."""
    size=len(data)
    chunk_size=1024
    sent=0

    print(f"[uploader] phase 3: sending data ({size} bytes)...")
    while sent < size:
        end=min(sent+chunk_size,size)
        ser.write(data[sent:end])
        sent=end
        print(
            f"[uploader] phase 3: progress {sent}/{size} bytes ({100*sent/size:.1f}%)",
            end='\r',
            flush=True,
        )
    ser.flush()
    print()  # 진행률 줄 마무리 (다음 줄로)
    print(f"[uploader] phase 3: data sent")

def send_crc(ser, crc):
    """Phase 4: send crc32 (4B little-endian), expect CRCOK!\\r\\n or CRCFAIL\\r\\n."""
    payload=struct.pack('<I', crc)
    
    print(f"[uploader] phase 4: sending crc32 (0x{crc:08X})...")
    ser.write(payload)
    ser.flush()
    
    response = ser.read_until(b'\n', size=64)
    print(f"[uploader] phase 4: received {response!r}")
    
    if response==b"CRCOK!\r\n":
        print(f"[uploader] phase 4: CRC OK")
        return
    if response==b"CRCFAIL!\r\n":
        raise RuntimeError(f"board rejected CRC: CRCFAIL")
    raise RuntimeError(f"phase 4 unexpected response: {response!r}")

def wait_for_done(ser):
    """Phase 5: stream flash progress logs, wait for DONE! or FLASHFAIL!."""
    print(f"[uploader] phase 5: waiting for flash completion...")
    while True:
        line=ser.read_until(b'\n', size=256)
        if not line:
            raise RuntimeError("phase 5: timeout waiting for response")
        
        text=line.decode('ascii', errors='replace').rstrip('\r\n')
        print(f"[uploader] phase 5: {text}")
        
        if line == b"DONE!\r\n":
            print(f"[uploader] phase 5: flash complete, board will reset")
            return
        if line == b"FLASHFAIL!\r\n":
            raise RuntimeError("board reported FLASHFAIL")

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
    ser = serial.Serial(args.port, args.baud, timeout=args.timeout,
                        dsrdtr=False, rtscts=False)
    print(f"[uploader] port open: {ser.name}")
    try:
        handshake(ser)
        send_size(ser, size)
        send_data(ser, data)
        send_crc(ser, crc)
        wait_for_done(ser)
    finally:
        ser.close()
        print(f"[uploader] port closed")




if __name__ == "__main__":
    sys.exit(main())
