#!/usr/bin/env python3
"""
FV1 Programmer Firmware Update Tool

Uploads a .bin firmware image to the STM32L053 bootloader over USB CDC serial.

Usage:
    python firmware_update.py /dev/ttyACM0 firmware.bin [--version 1]
"""

import argparse
import struct
import sys
import time
import zlib

import serial

# Protocol constants
APP_BASE_ADDR = 0x08004000
PAGE_SIZE = 128
APP_MAGIC = 0xDEADBEEF
MAX_APP_SIZE = 48 * 1024
RESPONSE_TIMEOUT = 2.0
BAUD_RATE = 115200  # ignored by CDC but required by pyserial


def read_response(ser, timeout=RESPONSE_TIMEOUT):
    """Read a newline-terminated response string."""
    ser.timeout = timeout
    data = ser.read_until(b'\n')
    if not data:
        return None
    return data.decode('ascii', errors='replace').strip()


def send_and_expect(ser, payload, expected, step_name):
    """Send payload bytes, read response, check it matches expected."""
    ser.write(payload)
    resp = read_response(ser)
    if resp is None:
        print(f"\n  ERROR: no response during {step_name} (timeout)")
        sys.exit(1)
    if resp != expected:
        print(f"\n  ERROR during {step_name}: expected '{expected}', got '{resp}'")
        sys.exit(1)
    return resp


def main():
    parser = argparse.ArgumentParser(description="FV1 Programmer Firmware Updater")
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyACM0, COM3)")
    parser.add_argument("firmware", help="Path to .bin firmware file")
    parser.add_argument("--version", type=int, default=1, help="Firmware version number (default: 1)")
    args = parser.parse_args()

    # Read firmware binary
    with open(args.firmware, "rb") as f:
        firmware = f.read()

    if len(firmware) == 0:
        print("ERROR: firmware file is empty")
        sys.exit(1)

    if len(firmware) > MAX_APP_SIZE:
        print(f"ERROR: firmware too large ({len(firmware)} bytes, max {MAX_APP_SIZE})")
        sys.exit(1)

    # Pad to page boundary
    remainder = len(firmware) % PAGE_SIZE
    if remainder != 0:
        firmware += b'\xFF' * (PAGE_SIZE - remainder)

    num_pages = len(firmware) // PAGE_SIZE
    fw_crc = zlib.crc32(firmware) & 0xFFFFFFFF

    print(f"Firmware: {args.firmware}")
    print(f"  Size: {len(firmware)} bytes ({num_pages} pages)")
    print(f"  CRC32: 0x{fw_crc:08X}")
    print(f"  Version: {args.version}")
    print()

    # Open serial port
    try:
        ser = serial.Serial(args.port, BAUD_RATE, timeout=RESPONSE_TIMEOUT)
    except serial.SerialException as e:
        print(f"ERROR: cannot open {args.port}: {e}")
        sys.exit(1)

    # Flush any stale data
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    # Step 1: Identify bootloader
    print("[1/6] Identifying bootloader...", end=" ", flush=True)
    send_and_expect(ser, b'B', "BOOTLOADER_V1.0", "identify")
    print("OK")

    # Step 2: Erase pages
    print(f"[2/6] Erasing {num_pages} pages...", end=" ", flush=True)
    for i in range(num_pages):
        addr = APP_BASE_ADDR + i * PAGE_SIZE
        payload = b'E' + struct.pack('<I', addr)
        send_and_expect(ser, payload, "ERASE_OK", f"erase page {i} @ 0x{addr:08X}")
        # Progress
        pct = (i + 1) * 100 // num_pages
        print(f"\r[2/6] Erasing {num_pages} pages... {pct}%", end="", flush=True)
    print(" OK")

    # Step 3: Program pages
    print(f"[3/6] Programming {num_pages} pages...", end=" ", flush=True)
    for i in range(num_pages):
        addr = APP_BASE_ADDR + i * PAGE_SIZE
        chunk = firmware[i * PAGE_SIZE : (i + 1) * PAGE_SIZE]
        payload = b'P' + struct.pack('<I', addr) + chunk
        send_and_expect(ser, payload, "PROG_OK", f"program page {i} @ 0x{addr:08X}")
        pct = (i + 1) * 100 // num_pages
        print(f"\r[3/6] Programming {num_pages} pages... {pct}%", end="", flush=True)
    print(" OK")

    # Step 4: Write metadata
    print("[4/6] Writing metadata...", end=" ", flush=True)
    meta = struct.pack('<IIII', APP_MAGIC, len(firmware), fw_crc, args.version)
    meta += b'\x00' * (PAGE_SIZE - len(meta))  # pad to 128 bytes
    payload = b'M' + meta
    send_and_expect(ser, payload, "META_OK", "write metadata")
    print("OK")

    # Step 5: Verify CRC
    print("[5/6] Verifying CRC...", end=" ", flush=True)
    payload = b'C' + struct.pack('<I', fw_crc)
    send_and_expect(ser, payload, "CRC_OK", "verify CRC")
    print("OK")

    # Step 6: Jump to application
    print("[6/6] Jumping to application...", end=" ", flush=True)
    ser.write(b'G')
    resp = read_response(ser)
    if resp and "JUMPING" in resp:
        print("OK")
    else:
        print(f"WARNING: unexpected response '{resp}' (device may have jumped anyway)")

    ser.close()
    print("\nFirmware update complete.")


if __name__ == "__main__":
    main()
