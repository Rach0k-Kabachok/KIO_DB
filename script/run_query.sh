#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/src/kio_db"

if [[ "$#" -ne 4 ]]; then
    echo "Usage: $0 <zero_based_query_id> <input_kiodb> <output_csv> <log_file>" >&2
    exit 1
fi

if [[ ! "$1" =~ ^[0-9]+$ ]]; then
    echo "Invalid query id: $1" >&2
    exit 1
fi

QUERY_ID=$((10#$1 + 1))

mkdir -p "$(dirname "$3")" "$(dirname "$4")"

"${BIN}" "query" "$2" "${QUERY_ID}" "$3" >"$4" 2>&1

echo "Query ${QUERY_ID} Complete"
