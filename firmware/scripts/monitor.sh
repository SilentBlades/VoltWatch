#!/usr/bin/env bash
set -euo pipefail

PORT="${PORT:-${1:-}}"
BAUD="${BAUD:-115200}"

if [ -z "$PORT" ]; then
    echo "Auto-detecting connected serial port..."
    DETECTED_PORT=$(arduino-cli board list | grep -E "usbserial|ttyUSB|cu.usb" | awk '{print $1}' | head -n 1 || true)
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
        echo "Detected target board at port: $PORT"
    else
        echo "Error: No target port specified and auto-detection found no devices."
        echo "Usage: $0 /dev/cu.usbserial-XXXX [BAUD]"
        exit 1
    fi
fi

echo "=================================================="
echo "Opening Serial Monitor on $PORT @ $BAUD baud"
echo "Press Ctrl+C to disconnect."
echo "=================================================="

arduino-cli monitor -p "$PORT" --config baudrate="$BAUD"
