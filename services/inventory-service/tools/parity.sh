#!/usr/bin/env bash
# Parity check: legacy bin/mediation + bin/inventory-api vs inventory-service on the same data drop.
#   services/inventory-service/tools/parity.sh [DATA_DIR]      (default: data/)
# Compares the mediation run report with the service's ingest_run report, then every
# contracts/http inventory request (sites*, circuits*, capacity*, inventory-*) plus extra probes,
# byte for byte: status code, body, Content-Type and Access-Control-Allow-Origin.
# Needs make, g++, libsqlite3-dev, curl, docker (Postgres 16) and Java 21 + Maven.
# Env: LEGACY_PORT (18081), NEW_PORT (18091), PG_PORT (15433),
#      POSTGRES_IMAGE (postgres:16-alpine; mirror.gcr.io/library/postgres:16-alpine when rate limited),
#      SKIP_BUILD=1 to reuse services/inventory-service/target/inventory-service-*.jar.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$ROOT"
DATA=$(cd "${1:-data}" && pwd)
LEGACY_PORT=${LEGACY_PORT:-18081}
NEW_PORT=${NEW_PORT:-18091}
PG_PORT=${PG_PORT:-15433}
POSTGRES_IMAGE=${POSTGRES_IMAGE:-postgres:16-alpine}
WORK=$(mktemp -d)
PG_NAME=inventory-parity-$$
PIDS=()
cleanup() {
  for p in "${PIDS[@]:-}"; do [[ -n "$p" ]] && kill "$p" 2>/dev/null || true; done
  docker rm -f "$PG_NAME" >/dev/null 2>&1 || true
}
trap cleanup EXIT

wait_http() {
  for _ in $(seq 600); do curl -s -o /dev/null "http://127.0.0.1:$1/" && return 0; sleep 0.2; done
  echo "port $1 did not come up" >&2; return 1
}

echo "== legacy: make + bin/mediation + bin/inventory-api"
make -s bin/mediation bin/inventory-api >/dev/null
./bin/mediation --data "$DATA" --db "$WORK/meridian.db" > "$WORK/legacy-mediation.txt"
./bin/inventory-api --db "$WORK/meridian.db" --locations "$DATA/locations.csv" --port "$LEGACY_PORT" 2>/dev/null & PIDS+=($!)

echo "== new: Postgres ($POSTGRES_IMAGE) + inventory-service"
docker run -d --rm --name "$PG_NAME" -p "127.0.0.1:$PG_PORT:5432" -e POSTGRES_DB=inventory \
  -e POSTGRES_USER=inventory -e POSTGRES_PASSWORD=inventory "$POSTGRES_IMAGE" >/dev/null
for _ in $(seq 100); do docker exec "$PG_NAME" pg_isready -q -U inventory -d inventory && break; sleep 0.3; done
if [[ "${SKIP_BUILD:-}" != 1 ]]; then
  mvn -B -q -ntp -pl services/inventory-service -am -DskipTests package
fi
JAR=$(ls services/inventory-service/target/inventory-service-*.jar | grep -v original | head -1)
INVENTORY_DB_URL="jdbc:postgresql://127.0.0.1:$PG_PORT/inventory" MERIDIAN_DATA_DIR="$DATA" \
  MERIDIAN_INGEST_ON_STARTUP=true java -jar "$JAR" --server.port="$NEW_PORT" > "$WORK/service.log" 2>&1 & PIDS+=($!)
wait_http "$LEGACY_PORT"
wait_http "$NEW_PORT"
# the startup ingest finishes before the port opens; this only guards against a failed run
for _ in $(seq 300); do
  [[ $(docker exec "$PG_NAME" psql -qAt -U inventory -d inventory \
    -c "SELECT count(*) FROM ingest_run WHERE finished_at IS NOT NULL" 2>/dev/null || echo 0) -gt 0 ]] && break
  sleep 0.2
done

FAIL=0
docker exec "$PG_NAME" psql -qAt -U inventory -d inventory \
  -c "SELECT report FROM ingest_run ORDER BY id DESC LIMIT 1" | head -c -1 > "$WORK/new-mediation.txt" # psql adds a newline
if diff -u "$WORK/legacy-mediation.txt" "$WORK/new-mediation.txt"; then
  echo "ok   mediation run report"
else
  echo "DIFF mediation run report"; FAIL=1
fi

fetch() { # port path out
  curl -s -D "$3.h" -o "$3.body" "http://127.0.0.1:$1$2"
  { head -1 "$3.h" | awk '{print $2}'
    grep -i -E '^(content-type|access-control-allow-origin):' "$3.h" | tr -d '\r' | tr 'A-Z' 'a-z' | sort
    cat "$3.body"; } > "$3"
}

PATHS=()
for req in contracts/http/{sites,circuits,capacity,inventory-}*.request; do PATHS+=("$(cat "$req")"); done
PATHS+=("/sites?STATUS_CD=1" "/sites?STATUS_CD=2" "/sites?REGION_CD=NOPE" "/circuits?CIRCUIT_ID=NOPE"
  "/sites?STATUS_CD=1e0" "/sites?STATUS_CD=+1" "/sites?STATUS_CD=0x3" "/sites?STATUS_CD=1.5"
  "/capacity?requested=0" "/capacity?requested=100000" "/capacity?market=NOPE" "/sites/x" "/")
i=0
for path in "${PATHS[@]}"; do
  i=$((i + 1))
  fetch "$LEGACY_PORT" "$path" "$WORK/legacy-$i"
  fetch "$NEW_PORT" "$path" "$WORK/new-$i"
  if cmp -s "$WORK/legacy-$i" "$WORK/new-$i"; then
    echo "ok   $path"
  else
    echo "DIFF $path"; diff <(fold -w 160 "$WORK/legacy-$i") <(fold -w 160 "$WORK/new-$i") | head -20 || true; FAIL=1
  fi
done

if [[ $FAIL == 0 ]]; then
  echo "inventory-service matches legacy on $DATA"
else
  echo "inventory-service differs from legacy on $DATA (service log: $WORK/service.log)"; exit 1
fi
