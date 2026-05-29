#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

apt-get update
apt-get install -y cmake build-essential git

mkdir -p "${ROOT_DIR}/build"
cd "${ROOT_DIR}/build"
cmake ..

echo "Setup complete"
