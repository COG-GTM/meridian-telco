#!/usr/bin/env bash
# Capture every observable output of the C++ meridian build into a golden
# directory (default parity/golden). Rerunning on unchanged inputs must give
# byte-identical files; see parity/README.md.
#
# usage: parity/capture.sh [OUT_DIR]
# env:   MERIDIAN_BIN   directory holding the binaries (default: bin)
#        SKIP_BUILD=1   do not run make first
#        INVENTORY_PORT / INVOICE_PORT   ports for the two services
#                       (default 18081 / 18082, so a running `make demo` is untouched)
set -euo pipefail
export LC_ALL=C TZ=UTC

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

OUT="${1:-parity/golden}"
BIN="${MERIDIAN_BIN:-bin}"
INVENTORY_PORT="${INVENTORY_PORT:-18081}"
INVOICE_PORT="${INVOICE_PORT:-18082}"

for tool in curl sqlite3; do
  command -v "$tool" >/dev/null || { echo "capture.sh: $tool is required" >&2; exit 1; }
done

if [ "${SKIP_BUILD:-0}" != 1 ]; then
  make -s all "$BIN/billing-test" >&2
fi

WORK="$(mktemp -d)"
PIDS=()
cleanup() {
  for p in "${PIDS[@]:-}"; do [ -n "$p" ] && kill "$p" 2>/dev/null || true; done
  wait 2>/dev/null || true
  rm -rf "$WORK"
}
trap cleanup EXIT

rm -rf "$OUT"
mkdir -p "$OUT"

# run_cli <out base> <cmd...>: writes <base>.stdout, <base>.stderr, <base>.exit
run_cli() {
  local base="$OUT/$1"; shift
  mkdir -p "$(dirname "$base")"
  local rc=0
  "$@" >"$base.stdout" 2>"$base.stderr" || rc=$?
  echo "$rc" >"$base.exit"
}

slug() {
  local s="${1#/}"
  s="${s//\?/__}"; s="${s//&/__}"; s="${s//=/-}"; s="${s//\//_}"
  [ -n "$s" ] || s="root"
  printf '%s' "$s"
}

# http_case <service dir> <port> <path>: raw response (status line, headers, body)
http_case() {
  local dir="$OUT/$1" port="$2" path="$3" name
  name="$(slug "$path")"
  mkdir -p "$dir"
  curl -sS -i --raw --path-as-is -o "$dir/$name.http" "http://127.0.0.1:$port$path"
  printf '%s\t%s\n' "$name.http" "$path" >>"$dir/cases.tsv"
}

start_server() {
  local log="$1" port="$2"; shift 2
  "$@" 2>"$log" &
  PIDS+=("$!")
  for _ in $(seq 1 100); do
    if curl -s -o /dev/null "http://127.0.0.1:$port/"; then return 0; fi
    if ! kill -0 "${PIDS[-1]}" 2>/dev/null; then break; fi
    sleep 0.05
  done
  echo "capture.sh: server on :$port did not come up" >&2
  cat "$log" >&2
  exit 1
}

stop_server() {
  local pid="${PIDS[-1]}"
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  unset 'PIDS[-1]'
}

server_log() {
  sed "s/:$2\$/:<port>/" "$WORK/$1.log" >"$OUT/$1/server.stderr"
}

# --- mediation -> meridian.db -------------------------------------------------
DB="$WORK/meridian.db"
run_cli mediation/run "$BIN/mediation" --data data --db "$DB"
run_cli mediation/missing-data "$BIN/mediation" --data does-not-exist --db "$WORK/missing.db"

mkdir -p "$OUT/db"
sqlite3 "$DB" .dump >"$OUT/db/meridian.dump.sql"
sqlite3 -header -csv "$DB" "SELECT * FROM SITE ORDER BY ASSET_ID" >"$OUT/db/SITE.csv"
sqlite3 -header -csv "$DB" "SELECT * FROM CIRCUIT ORDER BY CIRCUIT_ID" >"$OUT/db/CIRCUIT.csv"

# --- billing-run: register + reconciliation feed ------------------------------
PERIODS="$(tail -n +2 data/usage.csv | cut -d, -f2 | sort -u)"
run_cli billing-run/register "$BIN/billing-run" --accounts billing/accounts --usage data/usage.csv
run_cli billing-run/feed "$BIN/billing-run" --accounts billing/accounts --usage data/usage.csv --json
for p in $PERIODS 1999-01; do
  run_cli "billing-run/register-$p" "$BIN/billing-run" --period "$p"
  run_cli "billing-run/feed-$p" "$BIN/billing-run" --period "$p" --json
done

# --- billing-test, ipam -------------------------------------------------------
run_cli billing-test/run "$BIN/billing-test"
run_cli ipam/run "$BIN/ipam" --devices network/devices.csv
run_cli ipam/missing-devices "$BIN/ipam" --devices does-not-exist.csv

# --- inventory-api ------------------------------------------------------------
start_server "$WORK/inventory-api.log" "$INVENTORY_PORT" \
  "$BIN/inventory-api" --db "$DB" --locations data/locations.csv --port "$INVENTORY_PORT"
inv() { http_case inventory-api "$INVENTORY_PORT" "$1"; }
inv /sites
for r in $(tail -n +2 data/sites.csv | cut -d, -f4 | sort -u) XX-99; do inv "/sites?REGION_CD=$r"; done
for s in $(tail -n +2 data/sites.csv | cut -d, -f7 | sort -u) 9; do inv "/sites?STATUS_CD=$s"; done
inv "/sites?REGION_CD=NE-04&STATUS_CD=2"
inv /circuits
inv "/circuits?CIRCUIT_ID=$(sed -n 2p data/circuits.csv | cut -d, -f1)"
inv "/circuits?CIRCUIT_ID=CKT-NONE-000000"
inv /capacity
for n in 0 50 100 500 1000 10000 -50 abc; do inv "/capacity?requested=$n"; done
for m in $(tail -n +2 data/locations.csv | cut -d, -f4 | sort -u) NOPE-00; do
  inv "/capacity?market=$m"
  inv "/capacity?requested=500&market=$m"
done
inv /
inv /nope
stop_server
server_log inventory-api "$INVENTORY_PORT"

# --- invoice-api --------------------------------------------------------------
start_server "$WORK/invoice-api.log" "$INVOICE_PORT" \
  "$BIN/invoice-api" --accounts billing/accounts --usage data/usage.csv --port "$INVOICE_PORT"
invc() { http_case invoice-api "$INVOICE_PORT" "$1"; }
ACCTS="$(grep -h '^ACCT_ID=' billing/accounts/*.rec | cut -d= -f2 | sort -u)"
for a in $ACCTS; do
  for p in $PERIODS; do invc "/invoices?acct=$a&period=$p"; done
done
invc /invoices
invc "/invoices?acct="
for p in $PERIODS 1999-01; do invc "/invoices?period=$p"; done
for a in $(echo "$ACCTS" | head -3); do invc "/invoices?acct=$a"; done
invc "/invoices?acct=NOPE-000000"
invc "/invoices?acct=NOPE-000000&period=$(echo "$PERIODS" | head -1)"
invc "/invoices?acct=$(echo "$ACCTS" | head -1)&period=1999-01"
invc /
invc /nope
stop_server
server_log invoice-api "$INVOICE_PORT"

# --- manifest -----------------------------------------------------------------
(cd "$OUT" && find . -type f ! -name MANIFEST.sha256 | sort | xargs sha256sum >MANIFEST.sha256)
echo "captured $(wc -l <"$OUT/MANIFEST.sha256") files into $OUT" >&2
