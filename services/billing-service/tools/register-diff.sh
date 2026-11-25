#!/usr/bin/env bash
# Runs the legacy C++ bin/billing-run and the Java billing-run side by side for one period,
# in text and --json mode, and diffs their output byte for byte.
#
#   services/billing-service/tools/register-diff.sh 2026-07 [--usage FILE] [--accounts DIR]
#
# Builds bin/billing-run (make) and the billing-service jar (mvn package) when missing or stale.
# Exit status: 0 when both registers are identical, 1 on any difference.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MODULE="$ROOT/services/billing-service"
PERIOD="${1:?usage: register-diff.sh PERIOD [--usage FILE] [--accounts DIR]}"
shift
EXTRA=("$@")
OUT="${REGISTER_DIFF_OUT:-$MODULE/target/register-diff}"
mkdir -p "$OUT"
cd "$ROOT"

[[ -x bin/billing-run ]] || make -s bin/billing-run
JAR="$(ls "$MODULE"/target/billing-service-*.jar 2>/dev/null | grep -v '\.original$' | head -1 || true)"
if [[ -z "$JAR" || -n "$(find "$MODULE/src/main" "$MODULE/pom.xml" -newer "$JAR" -type f | head -1)" ]]; then
  mvn -B -q -ntp ${MAVEN_ARGS:-} -pl services/billing-service -am -DskipTests package
  JAR="$(ls "$MODULE"/target/billing-service-*.jar | grep -v '\.original$' | head -1)"
fi

status=0
for mode in text json; do
  flags=(--period "$PERIOD" "${EXTRA[@]}")
  ext=txt
  if [[ $mode == json ]]; then flags+=(--json); ext=jsonl; fi
  legacy="$OUT/legacy-$PERIOD.$ext"
  java="$OUT/java-$PERIOD.$ext"
  bin/billing-run "${flags[@]}" > "$legacy"
  java -jar "$JAR" billing-run "${flags[@]}" > "$java"
  lines=$(wc -l < "$legacy")
  if cmp -s "$legacy" "$java"; then
    echo "IDENTICAL $PERIOD $mode: $lines lines, sha256 $(sha256sum < "$legacy" | cut -c1-16)"
  else
    echo "DIFFERENT $PERIOD $mode:"
    diff -u "$legacy" "$java" | head -50 || true
    status=1
  fi
done
exit $status
