#!/usr/bin/env bash
# Replays every frozen request in contracts/http/ against running services and diffs status + body byte for byte.
#   INVENTORY=http://localhost:8081 BILLING=http://localhost:8082 tools/parity/http-diff.sh
set -uo pipefail
cd "$(dirname "$0")/../.."
INVENTORY=${INVENTORY:-http://localhost:8081}; BILLING=${BILLING:-http://localhost:8082}
fail=0; n=0
for req in contracts/http/*.request; do
  name=$(basename "$req" .request); base=$INVENTORY
  [[ $name == invoices* ]] && base=$BILLING
  got=$(mktemp)
  status=$(curl -s -o "$got" -w '%{http_code}' "$base$(cat "$req")")
  n=$((n+1))
  if [[ "$status" != "$(cat "contracts/http/$name.status")" ]] || ! cmp -s "$got" "contracts/http/$name.json"; then
    echo "DIFF $name (status $status, want $(cat "contracts/http/$name.status"))"; fail=$((fail+1))
  else
    echo "ok   $name"
  fi
  rm -f "$got"
done
echo "$((n-fail))/$n contracts identical"
exit $((fail > 0))
