#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "${BUILD_DIR}" --config Release --parallel

echo "Build Complete"
