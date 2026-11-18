#!/usr/bin/env python3
"""
Read FV1 program from EEPROM via USB serial
Works on both Linux and Mac by keeping connection open
"""

import sys
import subprocess
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

def read_fv1(device, output_file):
    try:
        # Open serial port and keep it open
        with serial.Serial(device, 115200, timeout=5) as ser:
            # Small delay to let device settle
            time.sleep(0.1)

            # Send READ command
            ser.write(b'R')
            ser.flush()

            # Read exactly 4096 bytes
            data = ser.read(4096)

            if len(data) != 4096:
                print(f"Warning: Read {len(data)} bytes (expected 4096)")
                return False

            # Write to output file
            with open(output_file, 'wb') as f:
                f.write(data)

            print(f"Success! Saved to {output_file} ({len(data)} bytes)")
            return True

    except serial.SerialException as e:
        print(f"Error: {e}")
        return False
    except Exception as e:
        print(f"Unexpected error: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: read_fv1.py [output_file] [device]")
        print("  output_file - Output file (default: current.fv1)")
        print("  device - Serial device (default: auto-detect)")
        sys.exit(1)

    output_file = sys.argv[1] if len(sys.argv) > 1 else "current.fv1"
    device = sys.argv[2] if len(sys.argv) > 2 else None

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
        print("Verifying devices...")
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
            print("  ./read_fv1.py output.fv1 /dev/ttyACM0  (Linux)")
            print("  ./read_fv1.py output.fv1 COM3         (Windows)")
            sys.exit(1)

        if len(fv1_ports) > 1:
            print("Multiple FV1 Programmers found:")
            for i, p in enumerate(fv1_ports):
                vid_pid = f"VID:PID={p.vid:04X}:{p.pid:04X}" if p.vid else "No VID/PID"
                print(f"  [{i}] {p.device}: {p.description} ({vid_pid})")
            print(f"\nUsing first device: {fv1_ports[0].device}")
            print("To use a different device, specify it manually:")
            print(f"  ./read_fv1.py output.fv1 {fv1_ports[1].device}")

        device = fv1_ports[0].device
        print(f"Found FV1 Programmer: {device}")

    success = read_fv1(device, output_file)
    sys.exit(0 if success else 1)
