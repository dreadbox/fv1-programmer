#!/bin/bash
#
# write_fv1.sh - Write FV1 program to EEPROM
#
# Usage:
#   ./write_fv1.sh <input_file> [device]
#
# Arguments:
#   input_file - FV1 program file to write (must be exactly 4096 bytes)
#   device     - Serial device (default: auto-detect)
#

set -e

# Check arguments
if [ -z "$1" ]; then
    echo "Usage: $0 <input_file> [device]"
    echo ""
    echo "Arguments:"
    echo "  input_file - FV1 program file to write (must be exactly 4096 bytes)"
    echo "  device     - Serial device (optional, auto-detect if not specified)"
    echo ""
    echo "Examples:"
    echo "  $0 program.fv1"
    echo "  $0 program.fv1 /dev/tty.usbmodem1234"
    exit 1
fi

INPUT_FILE="$1"
DEVICE="${2}"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file '$INPUT_FILE' not found"
    exit 1
fi

# Check file size (must be exactly 4096 bytes)
FILE_SIZE=$(stat -f%z "$INPUT_FILE" 2>/dev/null || stat -c%s "$INPUT_FILE")
if [ "$FILE_SIZE" -ne 4096 ]; then
    echo "Error: Input file must be exactly 4096 bytes (got $FILE_SIZE bytes)"
    exit 1
fi

# Auto-detect device if not specified
if [ -z "$DEVICE" ]; then
    if [ "$(uname)" == "Darwin" ]; then
        # Mac OS - look for cu.usbmodem* (use cu device for output)
        DEVICE=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
    else
        # Linux - look for ttyACM0
        DEVICE=$(ls /dev/ttyACM* 2>/dev/null | head -n 1)
    fi

    if [ -z "$DEVICE" ]; then
        echo "Error: Could not auto-detect USB serial device"
        echo "Please specify device manually:"
        echo "  Mac OS: ./write_fv1.sh $INPUT_FILE /dev/cu.usbmodemXXXX"
        echo "  Linux:  ./write_fv1.sh $INPUT_FILE /dev/ttyACM0"
        exit 1
    fi
fi

echo "Writing FV1 program to $DEVICE..."
echo "Input file: $INPUT_FILE ($FILE_SIZE bytes)"

# Configure serial port (115200 baud, 8N1, no flow control)
stty -F "$DEVICE" 115200 cs8 -cstopb -parenb -crtscts 2>/dev/null || \
stty -f "$DEVICE" 115200 cs8 -cstopb -parenb -crtscts 2>/dev/null

# Use bidirectional connection for better compatibility
exec 3<> "$DEVICE"

# Send write command followed by file contents
echo -n "W" >&3
cat "$INPUT_FILE" >&3

# Wait for response (with timeout)
sleep 1
if command -v timeout >/dev/null 2>&1; then
    RESPONSE=$(timeout 2 cat <&3 2>/dev/null || true)
else
    # No timeout command, read with head and timeout manually
    RESPONSE=$(head -c 10 <&3 2>/dev/null || true)
fi

exec 3>&-

if echo "$RESPONSE" | grep -q "OK"; then
    echo "Success! FV1 program written to EEPROM"
    exit 0
elif echo "$RESPONSE" | grep -q "ERROR"; then
    echo "Error: Write failed (device responded with ERROR)"
    exit 1
else
    echo "Warning: No response from device (write may have succeeded)"
    exit 0
fi
