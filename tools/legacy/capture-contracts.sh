#!/usr/bin/env bash
# Captures the frozen external contracts from the legacy C++ binaries.
#   tools/legacy/capture-contracts.sh          rewrite contracts/
#   tools/legacy/capture-contracts.sh --check  fail if the binaries drift from contracts/
set -euo pipefail
cd "$(dirname "$0")/../.."
OUT=contracts
if [[ "${1:-}" == "--check" ]]; then OUT=$(mktemp -d); fi
mkdir -p "$OUT/http" "$OUT/register" "$OUT/mediation" "$OUT/billing" "$OUT/ipam"

make -s all bin/billing-test >/dev/null
DB=$(mktemp -d)/meridian.db
./bin/mediation --data data --db "$DB" > "$OUT/mediation/mediation-run.txt"
./bin/billing-test > "$OUT/billing/billing-test.txt"
./bin/ipam > "$OUT/ipam/ipam-report.txt"

for p in 2026-06 2026-07; do
  ./bin/billing-run --period "$p" > "$OUT/register/register-$p.txt"
  ./bin/billing-run --period "$p" --json > "$OUT/register/register-$p.jsonl"
done
./bin/billing-run > "$OUT/register/register-all.txt"
./bin/billing-run --json > "$OUT/register/register-all.jsonl"

INV_PORT=18081; BILL_PORT=18082
./bin/inventory-api --db "$DB" --port $INV_PORT 2>/dev/null & P1=$!
./bin/invoice-api --port $BILL_PORT 2>/dev/null & P2=$!
trap 'kill $P1 $P2 2>/dev/null || true' EXIT
for port in $INV_PORT $BILL_PORT; do
  for _ in $(seq 50); do curl -s -o /dev/null "http://127.0.0.1:$port/" && break; sleep 0.1; done
done

# name|port|path  (status code is stored next to the body)
while IFS='|' read -r name port path; do
  [[ -z "$name" ]] && continue
  code=$(curl -s -o "$OUT/http/$name.json" -w '%{http_code}' "http://127.0.0.1:$port$path")
  echo "$path" > "$OUT/http/$name.request"
  echo "$code" > "$OUT/http/$name.status"
done <<LIST
sites|$INV_PORT|/sites
sites-region-ne04|$INV_PORT|/sites?REGION_CD=NE-04
sites-status-3|$INV_PORT|/sites?STATUS_CD=3
circuits|$INV_PORT|/circuits
circuits-by-id|$INV_PORT|/circuits?CIRCUIT_ID=CKT-NE04-000118
capacity|$INV_PORT|/capacity
capacity-requested-700|$INV_PORT|/capacity?requested=700
capacity-market-bos12|$INV_PORT|/capacity?market=BOS-12&requested=500
inventory-unknown-path|$INV_PORT|/nope
invoices-all|$BILL_PORT|/invoices
invoices-acct|$BILL_PORT|/invoices?acct=BEACON-004417
invoices-acct-period|$BILL_PORT|/invoices?acct=BEACON-004417&period=2026-07
invoices-period|$BILL_PORT|/invoices?period=2026-06
invoices-unknown-acct|$BILL_PORT|/invoices?acct=NOPE-000000
invoices-unknown-path|$BILL_PORT|/nope
LIST

if [[ "${1:-}" == "--check" ]]; then
  diff -r contracts "$OUT" --exclude=README.md && echo "legacy binaries match frozen contracts"
fi
