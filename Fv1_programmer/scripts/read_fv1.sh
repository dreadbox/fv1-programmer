#!/bin/bash
#
# read_fv1.sh - Read FV1 program from EEPROM
#
# Usage:
#   ./read_fv1.sh [output_file] [device]
#
# Arguments:
#   output_file - Output file to save EEPROM contents (default: current.fv1)
#   device      - Serial device (default: auto-detect)
#

set -e

# Default values
OUTPUT_FILE="${1:-current.fv1}"
DEVICE="${2}"

# Auto-detect device if not specified
if [ -z "$DEVICE" ]; then
    if [ "$(uname)" == "Darwin" ]; then
        # Mac OS - look for cu.usbmodem* (use cu device for I/O)
        DEVICE=$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
    else
        # Linux - look for ttyACM0
        DEVICE=$(ls /dev/ttyACM* 2>/dev/null | head -n 1)
    fi

    if [ -z "$DEVICE" ]; then
        echo "Error: Could not auto-detect USB serial device"
        echo "Please specify device manually:"
        echo "  Mac OS: ./read_fv1.sh $OUTPUT_FILE /dev/cu.usbmodemXXXX"
        echo "  Linux:  ./read_fv1.sh $OUTPUT_FILE /dev/ttyACM0"
        exit 1
    fi
fi

echo "Reading FV1 program from $DEVICE..."
echo "Output file: $OUTPUT_FILE"

# Configure serial port (115200 baud, 8N1, no flow control, no reset on open)
if [ "$(uname)" == "Darwin" ]; then
    stty -f "$DEVICE" 115200 cs8 -cstopb -parenb -crtscts
else
    # Linux: disable DTR/RTS to prevent reset on open
    stty -F "$DEVICE" 115200 cs8 -cstopb -parenb -crtscts -hupcl raw
fi

# Send command and read response
# Use same simple approach that works with TEST command
echo -n "R" > "$DEVICE"
sleep 0.5

# Read exactly 4096 bytes from device
head -c 4096 "$DEVICE" > "$OUTPUT_FILE" 2>/dev/null

FILE_SIZE=$(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE")

if [ "$FILE_SIZE" -eq 4096 ]; then
    echo "Success! Saved to $OUTPUT_FILE ($FILE_SIZE bytes)"
    exit 0
else
    echo "Warning: Read $FILE_SIZE bytes (expected 4096)"
    exit 1
fi
