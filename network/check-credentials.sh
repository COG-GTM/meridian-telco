#!/bin/sh
# fails if an snmp poller credential is committed in plaintext under network/.
# expected form: snmpv3 user env:NAME auth <alg> env:NAME priv <alg> env:NAME
status=0
for f in $(git ls-files network); do
  awk -v f="$f" '
    $1 == "community" { printf "%s:%d: plaintext snmp community string\n", f, NR; bad = 1 }
    $1 == "snmpv3" && ($3 !~ /^env:[A-Z0-9_]+$/ || $6 !~ /^env:[A-Z0-9_]+$/ || $9 !~ /^env:[A-Z0-9_]+$/) {
      printf "%s:%d: snmpv3 credential is not an env: reference\n", f, NR; bad = 1
    }
    END { exit bad }' "$f" || status=1
done
exit $status
