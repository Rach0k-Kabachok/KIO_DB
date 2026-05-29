#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

apt-get update
apt-get install -y cmake build-essential git

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DBUILD_TESTING=OFF

echo "Setup complete"
