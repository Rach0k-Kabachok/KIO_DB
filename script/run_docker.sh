#!/usr/bin/env bash
set -euo pipefail

docker run --rm \
  --network host \
  -v /home/ivan/Programming_repos/KIO_DB:/host_repo:ro \
  -v /home/ivan/kio-docker-test/data:/data:ro \
  -v /home/ivan/kio-docker-test/results:/results \
  kio-bench-test
