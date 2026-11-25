#!/usr/bin/env bash
# Runs the Java stack locally: Postgres (one DB per service) + Kafka, the three services and the dashboard.
#   tools/run-local.sh          build, start everything, wait until healthy
#   tools/run-local.sh stop     stop services and infra
# Images can be overridden (POSTGRES_IMAGE, KAFKA_IMAGE), e.g. to use a registry mirror.
set -euo pipefail
cd "$(dirname "$0")/.."
RUN=.run; mkdir -p "$RUN"

if [[ "${1:-}" == "stop" ]]; then
  for p in "$RUN"/*.pid; do [[ -f "$p" ]] && kill "$(cat "$p")" 2>/dev/null || true; rm -f "$p"; done
  docker compose -f ops/compose.yaml down -v
  exit 0
fi

docker compose -f ops/compose.yaml up -d --wait
[[ "${SKIP_BUILD:-}" == 1 ]] || mvn -B -q -ntp package -DskipTests

start() { # name jar env...
  local name=$1 jar=$2; shift 2
  env "$@" MERIDIAN_KAFKA_ENABLED=true java -jar "$jar" > "$RUN/$name.log" 2>&1 &
  echo $! > "$RUN/$name.pid"
}
start inventory services/inventory-service/target/inventory-service-*[0-9T].jar \
  MERIDIAN_DATA_DIR=data MERIDIAN_INGEST_ON_STARTUP=true
start billing services/billing-service/target/billing-service-*[0-9T].jar \
  BILLING_SEED_ACCOUNTS_DIR=billing/accounts
start network services/network-service/target/network-service-*[0-9T].jar \
  MERIDIAN_NETWORK_DIR=network
python3 -m http.server 8000 --directory dashboard > "$RUN/dashboard.log" 2>&1 &
echo $! > "$RUN/dashboard.pid"

for url in http://localhost:8081/capacity "http://localhost:8082/invoices?acct=BEACON-004417&period=2026-07" \
           http://localhost:8084/ipam/report http://localhost:8000/; do
  for _ in $(seq 120); do curl -sf -o /dev/null "$url" && break; sleep 1; done
  curl -sf -o /dev/null "$url" && echo "up   $url" || { echo "DOWN $url (see $RUN/*.log)"; exit 1; }
done
echo "dashboard: http://localhost:8000/"
