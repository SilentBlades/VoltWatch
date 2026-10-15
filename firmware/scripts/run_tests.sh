#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Building and running VoltWatch native unit tests..."
make -C "$PROJECT_ROOT/test" clean
make -C "$PROJECT_ROOT/test" test
