#!/usr/bin/env python3
"""
Write FV1 program to EEPROM via USB serial
Supports multiple files with concatenation and zero-padding
Works on both Linux and Mac by keeping connection open
"""

import sys
import subprocess
import os
import time

def install_package(package):
    """Try multiple methods to install a package"""
    methods = [
        [sys.executable, "-m", "pip", "install", package, "-q"],
        [sys.executable, "-m", "ensurepip", "--default-pip"],  # Bootstrap pip first
        ["pip3", "install", package, "-q"],
        ["pip", "install", package, "-q"],
    ]
    
    # Try ensurepip first if pip module fails
    for i, cmd in enumerate(methods):
        try:
            subprocess.check_call(cmd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
            if "ensurepip" in cmd:
                # After bootstrapping pip, try pip install again
                subprocess.check_call([sys.executable, "-m", "pip", "install", package, "-q"])
            return True
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
    return False

# Auto-install pyserial if missing
try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Installing required package: pyserial...")
    if install_package("pyserial"):
        import serial
        from serial.tools import list_ports
        print("Installation complete!\n")
    else:
        print("\nError: Could not install pyserial automatically.")
        print("Please install it manually using one of these commands:")
        print("  pip install pyserial")
        print("  pip3 install pyserial")
        print("  python3 -m pip install pyserial")
        print("  brew install python3  # macOS: ensures pip is available")
        sys.exit(1)

EEPROM_SIZE = 4096

def verify_device(device):
    """Verify that the device is actually an FV1 programmer"""
    try:
        with serial.Serial(device, 115200, timeout=1) as ser:
            time.sleep(0.1)
            ser.write(b'I')
            ser.flush()
            response = ser.read(50).decode('ascii', errors='ignore').strip()
            return 'FV1_PROGRAMMER' in response
    except:
        return False

def is_device_path(path):
    """Check if a path looks like a device rather than a file"""
    return (path.startswith('/dev/') or
            path.startswith('COM') or
            'usbmodem' in path.lower() or
            'ttyacm' in path.lower())

def load_and_concatenate_files(file_paths):
    """Load multiple files, concatenate them, and pad to 4KB"""
    data = bytearray()

    print(f"Loading {len(file_paths)} file(s):")
    for file_path in file_paths:
        if not os.path.exists(file_path):
            print(f"Error: File '{file_path}' not found")
            return None

        file_size = os.path.getsize(file_path)
        print(f"  {file_path}: {file_size} bytes")

        with open(file_path, 'rb') as f:
            data.extend(f.read())

    total_size = len(data)
    print(f"\nTotal size: {total_size} bytes")

    # Check for overflow
    if total_size > EEPROM_SIZE:
        print(f"Error: Total size ({total_size} bytes) exceeds EEPROM capacity ({EEPROM_SIZE} bytes)")
        print(f"Overflow: {total_size - EEPROM_SIZE} bytes")
        return None

    # Pad with zeros to exactly 4KB
    if total_size < EEPROM_SIZE:
        padding = EEPROM_SIZE - total_size
        data.extend(bytes(padding))
        print(f"Padded with {padding} zero bytes to reach {EEPROM_SIZE} bytes")

    return bytes(data)

def write_fv1(device, data):
    """Write data to FV1 EEPROM"""
    try:
        # Open serial port and keep it open
        with serial.Serial(device, 115200, timeout=5) as ser:
            # Small delay to let device settle
            time.sleep(0.1)

            print(f"\nWriting {len(data)} bytes to {device}...")

            # Send WRITE command
            ser.write(b'W')
            ser.flush()

            # Send all data
            ser.write(data)
            ser.flush()

            # Wait for response
            time.sleep(2)  # Give device time to write to EEPROM

            response = ser.read(10).decode('ascii', errors='ignore')

            if 'OK' in response:
                print("Success! FV1 program written to EEPROM")
                return True
            elif 'ERROR' in response:
                print("Error: Write failed (device responded with ERROR)")
                return False
            else:
                print("Warning: No response from device (write may have succeeded)")
                return True

    except serial.SerialException as e:
        print(f"Error: {e}")
        return False
    except Exception as e:
        print(f"Unexpected error: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: write_fv1.py <file1> [file2] [file3] ... [device]")
        print("")
        print("Examples:")
        print("  write_fv1.py program.fv1                    # Single file, auto-detect device")
        print("  write_fv1.py part1.bin part2.bin            # Multiple files, auto-detect device")
        print("  write_fv1.py program.fv1 /dev/ttyACM0       # Specify device")
        print("  write_fv1.py part1.bin part2.bin COM3       # Multiple files with device")
        print("")
        print("Notes:")
        print("  - Files are concatenated in the order specified")
        print("  - Total size must not exceed 4096 bytes")
        print("  - Data is padded with zeros to exactly 4096 bytes")
        print("  - Device is auto-detected if not specified")
        sys.exit(1)

    # Parse arguments - check if last argument is a device
    args = sys.argv[1:]
    device = None
    file_paths = args

    if len(args) > 1 and is_device_path(args[-1]):
        device = args[-1]
        file_paths = args[:-1]

    # Load and concatenate files
    data = load_and_concatenate_files(file_paths)
    if data is None:
        sys.exit(1)

    # Auto-detect device if not specified
    if not device:
        # Find all serial ports
        ports = list_ports.comports()

        # First try to find STM32 device by VID:PID (0x381E:0x000A is the FV-1 Programmer app)
        stm32_ports = [p for p in ports if p.vid == 0x381E and p.pid == 0x000A]

        # Fallback to any USB serial device
        if not stm32_ports:
            stm32_ports = [p for p in ports if 'USB' in p.description or 'ACM' in p.device or 'usbmodem' in p.device.lower()]

        # Verify each candidate is actually an FV1 programmer
        print("\nVerifying devices...")
        fv1_ports = [p for p in stm32_ports if verify_device(p.device)]

        if not fv1_ports:
            print("Error: No FV1 Programmer found")
            if stm32_ports:
                print(f"\nFound {len(stm32_ports)} STM32 device(s) but none responded as FV1 Programmer")
            print("\nAvailable ports:")
            for p in ports:
                vid_pid = f"VID:PID={p.vid:04X}:{p.pid:04X}" if p.vid else "No VID/PID"
                print(f"  {p.device}: {p.description} ({vid_pid})")
            print("\nPlease specify device manually:")
            print("  ./write_fv1.py program.fv1 /dev/ttyACM0  (Linux)")
            print("  ./write_fv1.py program.fv1 COM3         (Windows)")
            sys.exit(1)

        if len(fv1_ports) > 1:
            print("Multiple FV1 Programmers found:")
            for i, p in enumerate(fv1_ports):
                vid_pid = f"VID:PID={p.vid:04X}:{p.pid:04X}" if p.vid else "No VID/PID"
                print(f"  [{i}] {p.device}: {p.description} ({vid_pid})")
            print(f"\nUsing first device: {fv1_ports[0].device}")
            print("To use a different device, specify it manually:")
            print(f"  ./write_fv1.py {' '.join(file_paths)} {fv1_ports[1].device}")

        device = fv1_ports[0].device
        print(f"Found FV1 Programmer: {device}")

    success = write_fv1(device, data)
    sys.exit(0 if success else 1)
