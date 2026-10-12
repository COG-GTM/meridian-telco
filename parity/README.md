parity golden outputs
=====================

`golden/` is a byte-for-byte capture of everything the C++ build observably
produces from the inputs in this repo (`data/`, `billing/accounts/`,
`network/devices.csv`). It is committed so the Python port can be checked
against it after the C++ tree is gone.

regenerate:  parity/capture.sh            (builds with make, writes parity/golden)
             parity/capture.sh OUT_DIR    (write somewhere else, e.g. to diff)

env: MERIDIAN_BIN (binary dir, default bin), SKIP_BUILD=1,
     INVENTORY_PORT / INVOICE_PORT (default 18081 / 18082).
needs: g++, libsqlite3-dev, sqlite3 CLI, curl. The db dump depends on the
sqlite3 CLI's float formatting; golden/ was captured with sqlite3 3.37.2
(Ubuntu 22.04), which is what CI pins.

Running the script twice gives identical files (CI checks this, and that the
committed golden/ matches a fresh capture). Any change to an input or to the
C++ has to come with a recapture.

layout
------

CLI runs are stored as `<name>.stdout`, `<name>.stderr`, `<name>.exit`.

  mediation/run             bin/mediation --data data --db meridian.db
  mediation/missing-data    same with a data dir that does not exist
  db/meridian.dump.sql      sqlite3 meridian.db .dump of what mediation wrote
  db/SITE.csv               SELECT * FROM SITE ORDER BY ASSET_ID (sqlite3 -header -csv)
  db/CIRCUIT.csv            SELECT * FROM CIRCUIT ORDER BY CIRCUIT_ID
  billing-run/register      text register, no --period filter
  billing-run/feed          --json reconciliation feed, no --period filter
  billing-run/register-P    --period P, for every period in data/usage.csv
  billing-run/feed-P        --period P --json   (plus 1999-01, a period with no usage)
  billing-test/run          bin/billing-test
  ipam/run                  bin/ipam --devices network/devices.csv
  ipam/missing-devices      same with a devices file that does not exist

HTTP responses are stored raw as `<case>.http` (status line, headers, body,
exactly as sent). `cases.tsv` in each directory maps file -> request path.

  inventory-api/   /sites (all, every REGION_CD, every STATUS_CD, unknown values,
                   both params), /circuits (all, one id, unknown id),
                   /capacity (requested = none, 0 50 100 500 1000 10000 -50 abc;
                   every market with and without requested=500, unknown market),
                   / and /nope (404)
  invoice-api/     /invoices?acct=A&period=P for every account x period,
                   /invoices, ?acct= (empty), ?period=P only, ?acct=A only,
                   unknown acct (404), known acct with unknown period (404),
                   / and /nope (404)
  */server.stderr  service startup log, listen port replaced with <port>

`MANIFEST.sha256` lists every captured file with its sha256.
