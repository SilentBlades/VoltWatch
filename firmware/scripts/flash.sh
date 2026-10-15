#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

PORT="${PORT:-${1:-}}"
FQBN="${FQBN:-esp32:esp32:esp32:PartitionScheme=min_spiffs}"
FLASH_BAUD="${FLASH_BAUD:-460800}"
BUILD_DIR="${PROJECT_ROOT}/build"
BIN_FILE="${BUILD_DIR}/firmware.ino.bin"

if [ -z "$PORT" ]; then
    echo "Auto-detecting connected serial port..."
    DETECTED_PORT=$(arduino-cli board list | grep -E "usbserial|ttyUSB|cu.usb" | awk '{print $1}' | head -n 1 || true)
    if [ -n "$DETECTED_PORT" ]; then
        PORT="$DETECTED_PORT"
        echo "Detected target board at port: $PORT"
    else
        echo "Error: No target port specified and auto-detection found no devices."
        echo "Usage: make flash PORT=/dev/cu.usbserial-XXXX [FLASH_BAUD=460800]"
        exit 1
    fi
fi

if [ ! -f "$BIN_FILE" ]; then
    echo "Compiled binary ($BIN_FILE) not found. Triggering build first..."
    "$SCRIPT_DIR/build.sh"
fi

echo "=================================================="
echo "Uploading VoltWatch Firmware to $PORT"
echo "FQBN:       $FQBN"
echo "Baud Rate:  $FLASH_BAUD"
echo "Binary:     $BIN_FILE"
echo "=================================================="

arduino-cli upload \
    -p "$PORT" \
    --fqbn "$FQBN" \
    --upload-property "upload.speed=${FLASH_BAUD}" \
    --input-dir "$BUILD_DIR" \
    "$PROJECT_ROOT"

echo "Upload completed successfully."
