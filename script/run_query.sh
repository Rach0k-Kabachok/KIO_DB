#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BIN="${ROOT_DIR}/build/src/kio_db"

ZERO_BASED_QUERY_ID="$1"
INPUT_KIODB="$2"
OUTPUT_CSV="$3"
LOG_FILE="$4"
QUERY_ID=$((10#$1 + 1))
TIMES_CSV="${QUERY_TIMES_CSV:-$(dirname "${LOG_FILE}")/query_times.csv}"

mkdir -p "$(dirname "${OUTPUT_CSV}")" "$(dirname "${LOG_FILE}")"
mkdir -p "$(dirname "${TIMES_CSV}")"
if [[ ! -f "${TIMES_CSV}" ]]; then
    echo "query,time_ms" >"${TIMES_CSV}"
fi

START_NS="$(date +%s%N)"
set +e
"${BIN}" "query" "${INPUT_KIODB}" "${QUERY_ID}" "${OUTPUT_CSV}" >"${LOG_FILE}" 2>&1
STATUS=$?
set -e
END_NS="$(date +%s%N)"
ELAPSED_MS=$(((END_NS - START_NS) / 1000000))

echo "${ZERO_BASED_QUERY_ID},${ELAPSED_MS}" >>"${TIMES_CSV}"

if [[ "${STATUS}" -eq 0 ]]; then
    echo "Query ${QUERY_ID} Complete (${ELAPSED_MS} ms)"
    echo "Query ${QUERY_ID} time_ms=${ELAPSED_MS}" >>"${LOG_FILE}"
else
    echo "Query ${QUERY_ID} Failed (${ELAPSED_MS} ms). See ${LOG_FILE}" >&2
    echo "Query ${QUERY_ID} failed time_ms=${ELAPSED_MS}" >>"${LOG_FILE}"
    exit "${STATUS}"
fi
