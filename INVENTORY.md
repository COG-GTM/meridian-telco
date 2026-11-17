# Meridian OSS inventory

Paths and defaults below are relative to the repository working directory.

## mediation
- Sources: `mediation/{ingest,capacity,status,circuit_counter,store,locations}.cpp`.
- CLI/files: `--data data` reads `sites.csv`, `circuits.csv`; `--db meridian.db`. `make run` invokes these defaults. No ports/HTTP.
- DB: drops/recreates and writes `SITE`, `CIRCUIT`; no SELECTs in the ingest path.
- Dependencies: C++ standard library, SQLite3 (`-lsqlite3`, `sqlite3.h`); `mediation/{store,csv,capacity,status,circuit_counter,locations}.h`.
- Legacy: manual SQLite handle/error-buffer lifecycle (Store destructor does close), string-built SQL and unchecked execution results, fixed C buffers, raw pointers/zero nulls, ad-hoc CSV parser and numeric conversions.

## inventory-api
- Sources: `inventory-api/main.cpp` plus the same mediation sources except `ingest.cpp`.
- CLI/files: `--db meridian.db`, `--locations data/locations.csv`, `--port 8081`; loads locations once at startup.
- HTTP: GET `/sites` (optional `REGION_CD` or, only if region absent, `STATUS_CD`); GET `/circuits` (optional `CIRCUIT_ID`); GET `/capacity` (optional `requested` Mbps, `market`).
- DB: SELECTs `SITE`, `CIRCUIT`; no table writes (SQLite open can create an empty DB file). Capacity uses CSV, not DB.
- Dependencies: SQLite3, standard library/POSIX sockets; `inventory-api/httpd.h`, mediation headers listed above.
- Legacy: shared hand-rolled blocking HTTP server, global mutable state, interpolated query parameters in SQL, fixed buffers/manual JSON, unvalidated numeric inputs and manual SQLite management.

## billing-run
- Sources: `billing/run_billing.cpp` plus `billing/{rating,discounts,accounts,tax,proration,promo,suspension,lines,latefee,invoice}.cpp` (shared billing sources).
- CLI/files: `--accounts billing/accounts` reads `*.rec` key=value files; `--usage data/usage.csv`; optional `--period YYYY-MM`; `--json` emits one JSON object per line instead of the text register. `make register` selects `2026-07`.
- DB/HTTP: none; does not use `meridian.db`. `data/accounts.csv` is not the account-master input.
- Dependencies: C++ standard library, POSIX `dirent.h`; corresponding billing headers plus `money.h`, `datecalc.h`, `mediation/csv.h`. No external link libraries specified.
- Legacy: C-style numeric parsing/formatting, hand-written record/CSV/JSON handling, double-based money, linear account lookup, manual directory handle management.

## invoice-api
- Sources: `billing/invoice-api/main.cpp` plus shared billing sources above.
- CLI/files: `--accounts billing/accounts`, `--usage data/usage.csv`, `--port 8082`; loads inputs once at startup.
- HTTP: GET `/invoices`, optional `acct` and `period`; empty results with a nonempty account filter return 404, otherwise empty results return 200.
- DB: none; computes invoices directly from account records and usage.
- Dependencies: shared billing headers above and `inventory-api/httpd.h`; standard library/POSIX sockets and directory functions; no SQLite link.
- Legacy: same billing parsing/money limitations; global snapshots, hand-built JSON/fixed buffers, shared blocking HTTP implementation.

## ipam
- Sources: `network/ipam.cpp`.
- CLI/files: `--devices network/devices.csv`; prints duplicate addresses, counts per /24, and usage within hard-coded reserved `10.20.250.0/24` of `10.20.0.0/16`.
- DB/HTTP: none. Does not read `network/{reserved-ranges.conf,snmp-polling.conf,routing-tables/,firewall-rules/,dns-zones/}`; these are separate manually maintained artifacts.
- Dependencies: standard library and `mediation/csv.h` only.
- Legacy: unchecked sscanf IPv4 conversion, fixed buffers, hard-coded network/reservation, shared ad-hoc CSV parser.

## Dashboard pages
- `dashboard/index.html`: GET `http://localhost:8081/sites` and `/circuits`.
- `dashboard/billing.html`: GET `http://localhost:8082/invoices?period=...`, optional `&acct=...`; offers `2026-07` and `2026-06`.
- `dashboard/capacity.html`: GET `http://localhost:8081/capacity` once; requested bandwidth, market and text filtering run client-side (it does not send requested/market parameters).
- Static files; `make demo` serves dashboard on 8083 with Python http.server and starts invoice-api on 8082, but does not start inventory-api.

## Shared modernization surface
- Makefile uses `g++ -O2 -Wall` with no explicit `-std=` (compiler default, not a pinned C++ standard); C++98-style loops/raw pointers/zero nulls rather than modern ownership/nullptr. No claim that literal `NULL` is required by these sources.
- Makefile has no generated header dependency tracking, install/package layer or sanitizer configuration.
- `make check` builds `billing/rules_test.cpp`, a custom printf/counter-based harness, against billing sources; no inventory/HTTP/IPAM tests are wired into that target.
- `httpd.h` prefix-matches paths and parses but does not enforce HTTP methods (listed GETs are intended usage); no URL decoding, TLS, authentication, concurrency or robust request/partial-I/O handling. Unknown paths return JSON 404. JSON escaping only handles quote/backslash.
