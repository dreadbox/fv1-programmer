#!/usr/bin/env python3
"""Send 'U' command to the app to reboot into bootloader."""

import argparse
import serial
import sys

parser = argparse.ArgumentParser(description="Reboot FV1 Programmer into bootloader")
parser.add_argument("port", help="Serial port (e.g. /dev/cu.usbmodem0BAC323433351)")
args = parser.parse_args()

try:
    ser = serial.Serial(args.port, 115200, timeout=2)
    ser.write(b'U')
    ser.close()
    print("Sent 'U' -- device should reboot into bootloader.")
except serial.SerialException as e:
    print(f"ERROR: {e}")
    sys.exit(1)
