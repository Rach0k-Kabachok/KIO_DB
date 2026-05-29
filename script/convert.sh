#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/src/kio_db"

if [[ "$#" -ne 2 ]]; then
    echo "Usage: $0 <input_csv> <output_kiodb>" >&2
    exit 1
fi

mkdir -p "$(dirname "$2")"

"${BIN}" "convert" "$1" "${ROOT_DIR}/tests/hits_schema.csv" "$2"

echo "Convert Complete"
