#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

FQBN="${FQBN:-esp32:esp32:esp32:PartitionScheme=min_spiffs}"
BUILD_DIR="${PROJECT_ROOT}/build"

echo "=================================================="
echo "Compiling VoltWatch Firmware"
echo "FQBN:       $FQBN"
echo "Project:    $PROJECT_ROOT"
echo "Build Dir:  $BUILD_DIR"
echo "=================================================="

# Note: VoltWatch operates in headless API mode; static assets are not linked into flash.

arduino-cli compile \
    --fqbn "$FQBN" \
    --build-path "$BUILD_DIR" \
    --verbose \
    "$PROJECT_ROOT"

echo "Build succeeded. Output artifacts located in $BUILD_DIR"
