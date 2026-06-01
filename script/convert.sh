#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/src/kio_db"

INPUT_CSV="$1"
OUTPUT_KIODB="$2"
TIMES_CSV="${CONVERT_TIMES_CSV:-$(dirname "${OUTPUT_KIODB}")/convert_time.csv}"

mkdir -p "$(dirname "${OUTPUT_KIODB}")"
mkdir -p "$(dirname "${TIMES_CSV}")"
if [[ ! -f "${TIMES_CSV}" ]]; then
    echo "operation,time_ms" >"${TIMES_CSV}"
fi

START_NS="$(date +%s%N)"
set +e
"${BIN}" "convert" "${INPUT_CSV}" "${ROOT_DIR}/tests/hits_schema.csv" "${OUTPUT_KIODB}"
STATUS=$?
set -e
END_NS="$(date +%s%N)"
ELAPSED_MS=$(((END_NS - START_NS) / 1000000))

echo "convert,${ELAPSED_MS}" >>"${TIMES_CSV}"

if [[ "${STATUS}" -eq 0 ]]; then
    echo "Convert Complete (${ELAPSED_MS} ms)"
else
    echo "Convert Failed (${ELAPSED_MS} ms)" >&2
    exit "${STATUS}"
fi
