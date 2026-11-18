# FV1 Programmer Scripts

These scripts provide a simple command-line interface for reading and writing FV1 programs to the EEPROM via the USB CDC serial interface.

## Protocol

The device uses a simple binary protocol:
- **Read command**: Send `'R'` (0x52) → Receive 4096 bytes (EEPROM contents)
- **Write command**: Send `'W'` (0x57) followed by 4096 bytes → Receive `"OK\n"` or `"ERROR\n"`

## LED Indicators

- **Slow blink (1s on/off)**: Idle - ready for commands
- **Fast blink (100ms on/off)**: Write in progress
- **Solid on (2 seconds)**: Write successful
- **Triple blink pattern**: Write error

## Usage

### Mac OS / Linux

#### Read EEPROM
```bash
# Auto-detect device, save to current.fv1
./read_fv1.sh

# Specify output file
./read_fv1.sh my_program.fv1

# Specify device manually
./read_fv1.sh my_program.fv1 /dev/tty.usbmodem1234
```

#### Write EEPROM
```bash
# Auto-detect device
./write_fv1.sh program.fv1

# Specify device manually
./write_fv1.sh program.fv1 /dev/tty.usbmodem1234
```

**Note**: Input file must be exactly 4096 bytes.

### Windows

#### Read EEPROM
```batch
REM List available COM ports and read
read_fv1.bat

REM Read to specific file and COM port
read_fv1.bat my_program.fv1 COM3
```

#### Write EEPROM
```batch
REM List available COM ports
write_fv1.bat program.fv1

REM Write to specific COM port
write_fv1.bat program.fv1 COM3
```

**Note**: Input file must be exactly 4096 bytes.

## Device Detection

### Mac OS
The device appears as `/dev/tty.usbmodemXXXX` where XXXX is a device-specific ID.

List devices:
```bash
ls /dev/tty.usbmodem*
```

### Linux
The device appears as `/dev/ttyACM0` (or ttyACM1, ttyACM2, etc).

List devices:
```bash
ls /dev/ttyACM*
```

### Windows
The device appears as a COM port (e.g., COM3, COM4).

List devices:
```batch
mode
```

Or check Device Manager under "Ports (COM & LPT)".

## Manual Usage (without scripts)

You can also use standard serial terminal tools:

### Using screen (Mac/Linux)
```bash
# Read EEPROM
screen /dev/tty.usbmodem1234 115200
# Press 'R', then Ctrl+A, then ':' and type 'log current.fv1', press Enter
# Press Ctrl+A, then 'k' to quit

# Write EEPROM (more complex, recommend using script)
```

### Using cu (Mac/Linux)
```bash
# Connect
cu -l /dev/tty.usbmodem1234 -s 115200

# Type 'R' to read, or 'W' followed by binary data to write
# Type ~. to exit
```

### Using PuTTY (Windows)
1. Open PuTTY
2. Select "Serial" connection type
3. Set serial line to COM3 (or appropriate port)
4. Set speed to 115200
5. Click "Open"
6. Type 'R' to read or 'W' to write (followed by 4096 bytes)

## Troubleshooting

### "Could not auto-detect USB serial device"
- Make sure the device is connected via USB
- Check that the firmware has been flashed correctly
- On Linux, you may need permissions: `sudo usermod -a -G dialout $USER` (then logout/login)

### "Permission denied" (Linux/Mac)
```bash
# Add yourself to the dialout group (Linux)
sudo usermod -a -G dialout $USER

# On Mac, try with sudo or check permissions
ls -l /dev/tty.usbmodem*
```

### "Input file must be exactly 4096 bytes"
The FV1 EEPROM is exactly 4KB. If your file is smaller, you can pad it:
```bash
# Pad file to 4096 bytes with zeros
dd if=program.fv1 of=program_padded.fv1 bs=4096 conv=sync
```

### Write fails with "ERROR"
- Check that the EEPROM is working correctly
- Try reading first to verify communication
- Check LED indicators for status

## Examples

```bash
# Complete workflow on Mac OS

# 1. Read current EEPROM contents (backup)
./read_fv1.sh backup.fv1

# 2. Write new program
./write_fv1.sh new_program.fv1

# 3. Verify by reading back
./read_fv1.sh verify.fv1

# 4. Compare files
diff new_program.fv1 verify.fv1
```

## Technical Details

- **Baud rate**: 115200
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Flow control**: None
- **EEPROM size**: 4096 bytes (4KB)
- **EEPROM type**: 24LC32AT
