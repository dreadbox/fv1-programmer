#!/usr/bin/env python3
"""
Check if device is an FV1 Programmer and show info
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

def get_device_info(device):
    """Get device information"""
    try:
        with serial.Serial(device, 115200, timeout=1) as ser:
            time.sleep(0.1)

            # Send INFO command
            ser.write(b'I')
            ser.flush()
            info = ser.read(50).decode('ascii', errors='ignore').strip()

            return info
    except serial.SerialException as e:
        return f"Error: {e}"
    except Exception as e:
        return f"Unexpected error: {e}"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        # Auto-detect
        ports = list_ports.comports()
        stm32_ports = [p for p in ports if p.vid == 0x381E]

        if not stm32_ports:
            stm32_ports = [p for p in ports if 'USB' in p.description or 'ACM' in p.device or 'usbmodem' in p.device.lower()]

        if not stm32_ports:
            print("No USB serial devices found")
            sys.exit(1)

        print("Checking devices:\n")
        found_fv1 = False
        for p in stm32_ports:
            info = get_device_info(p.device)
            vid_pid = f"VID:PID={p.vid:04X}:{p.pid:04X}" if p.vid else "No VID/PID"
            print(f"{p.device}: {p.description} ({vid_pid})")
            print(f"  Response: {info}")
            if 'FV1_PROGRAMMER' in info:
                print(f"  ✓ This is an FV1 Programmer!")
                found_fv1 = True
            else:
                print(f"  ✗ Not an FV1 Programmer")
            print()

        sys.exit(0 if found_fv1 else 1)
    else:
        # Check specific device
        device = sys.argv[1]
        print(f"Checking {device}...")
        info = get_device_info(device)
        print(f"Response: {info}")

        if 'FV1_PROGRAMMER' in info:
            print("✓ This is an FV1 Programmer!")
            sys.exit(0)
        else:
            print("✗ Not an FV1 Programmer")
            sys.exit(1)
