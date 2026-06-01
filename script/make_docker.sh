#!/usr/bin/env bash
set -euo pipefail

mkdir -p /home/ivan/kio-docker-test/scripts
mkdir -p /home/ivan/kio-docker-test/data
mkdir -p /home/ivan/kio-docker-test/results

cat > /home/ivan/kio-docker-test/Dockerfile <<'EOF'
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates bash \
    && rm -rf /var/lib/apt/lists/*

COPY scripts/commands.sh scripts/env.sh /bench/
RUN chmod +x /bench/commands.sh

WORKDIR /work

ENTRYPOINT ["bash", "/bench/commands.sh"]
EOF

cat > /home/ivan/kio-docker-test/scripts/commands.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

source /bench/env.sh

mkdir repo
tar -C /host_repo \
    --exclude=.git \
    --exclude=build \
    --exclude=build-* \
    --exclude=cmake-build-debug \
    --exclude=.cache \
    --exclude=.idea \
    --exclude=.codex \
    --exclude=.agents \
    --exclude=Testing \
    -cf - . | tar -C repo -xf -

cd repo

./script/setup.sh
./script/build.sh

CONVERT_TIMES_CSV="${RESULTS}/convert_time.csv"
echo "operation,time_ms" > "${CONVERT_TIMES_CSV}"
export CONVERT_TIMES_CSV
./script/convert.sh "${INPUT_CSV}" "${COLUMNAR}"

TIMES_CSV="${RESULTS}/query_times.csv"
echo "query,time_ms" > "${TIMES_CSV}"
export QUERY_TIMES_CSV="${TIMES_CSV}"

for QUERY_NUM in {0..42}; do
    OUTPUT="${RESULTS}/query_${QUERY_NUM}.csv"
    LOGS="${RESULTS}/query_${QUERY_NUM}.log"

    ./script/run_query.sh "${QUERY_NUM}" "${COLUMNAR}" "${OUTPUT}" "${LOGS}"
done

echo "Query times saved to ${TIMES_CSV}"
EOF

cat > /home/ivan/kio-docker-test/scripts/env.sh <<'EOF'
export INPUT_CSV=/data/hits.csv
export COLUMNAR=/results/hits.kio
export RESULTS=/results
EOF

docker build --network host -t kio-bench-test /home/ivan/kio-docker-test

cp /home/ivan/Programming_repos/KIO_DB/tests/hits.csv /home/ivan/kio-docker-test/data/hits.csv
