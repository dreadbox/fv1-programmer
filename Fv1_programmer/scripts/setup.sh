#!/bin/bash
#
# FV1 Programmer Scripts Setup
# Creates a virtual environment and installs dependencies
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

echo "=== FV1 Programmer Scripts Setup ==="
echo ""

# Check if Python 3 is available
if command -v python3 &> /dev/null; then
    PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
    # Check if it's Python 3
    if python --version 2>&1 | grep -q "Python 3"; then
        PYTHON_CMD="python"
    else
        echo "Error: Python 3 is required but not found."
        echo ""
        echo "Please install Python 3:"
        echo "  macOS:   brew install python3"
        echo "  Ubuntu:  sudo apt install python3 python3-venv"
        echo "  Windows: Download from https://python.org"
        exit 1
    fi
else
    echo "Error: Python is not installed."
    echo ""
    echo "Please install Python 3:"
    echo "  macOS:   brew install python3"
    echo "  Ubuntu:  sudo apt install python3 python3-venv"
    echo "  Windows: Download from https://python.org"
    exit 1
fi

echo "Using: $($PYTHON_CMD --version)"
echo ""

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment..."
    $PYTHON_CMD -m venv "$VENV_DIR"
    if [ $? -ne 0 ]; then
        echo ""
        echo "Error: Failed to create virtual environment."
        echo "On Ubuntu/Debian, you may need to install python3-venv:"
        echo "  sudo apt install python3-venv"
        exit 1
    fi
    echo "Virtual environment created at: $VENV_DIR"
else
    echo "Virtual environment already exists at: $VENV_DIR"
fi

echo ""

# Activate and install dependencies
echo "Installing dependencies..."
source "$VENV_DIR/bin/activate"
pip install --upgrade pip -q
pip install pyserial -q

if [ $? -eq 0 ]; then
    echo "Dependencies installed successfully!"
else
    echo "Error: Failed to install dependencies."
    exit 1
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "To use the scripts, either:"
echo ""
echo "1. Activate the virtual environment first:"
echo "   source $VENV_DIR/bin/activate"
echo "   python read_fv1.py output.fv1"
echo "   python write_fv1.py program.fv1"
echo ""
echo "2. Or use the wrapper scripts (recommended):"
echo "   ./fv1_read.sh output.fv1"
echo "   ./fv1_write.sh program.fv1"
echo "   ./fv1_info.sh"
echo ""
