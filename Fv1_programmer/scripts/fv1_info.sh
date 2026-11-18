#!/bin/bash
#
# Wrapper script for fv1_info.py
# Automatically uses the virtual environment
#

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

# Check if venv exists, if not run setup
if [ ! -d "$VENV_DIR" ]; then
    echo "Virtual environment not found. Running setup..."
    "$SCRIPT_DIR/setup.sh"
    if [ $? -ne 0 ]; then
        exit 1
    fi
    echo ""
fi

# Run the Python script with the venv Python
"$VENV_DIR/bin/python" "$SCRIPT_DIR/fv1_info.py" "$@"
