#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/src/kio_db"

"${BIN}" "convert" "$1" "${ROOT_DIR}/tests/hits_schema.csv" "$2"

echo "Convert Complete"
