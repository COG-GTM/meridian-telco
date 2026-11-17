# Meridian OSS inventory

Paths and defaults below are relative to the repository working directory.

## mediation
- Sources: `mediation/{ingest,capacity,status,circuit_counter,store,locations}.cpp`.
- CLI/files: `--data data` reads `sites.csv`, `circuits.csv`; `--db meridian.db`. `cmake --build build --target run` invokes these defaults. No ports/HTTP.
- DB: drops/recreates and writes `SITE`, `CIRCUIT`; no SELECTs in the ingest path.
- Dependencies: C++ standard library, SQLite3 (`-lsqlite3`, `sqlite3.h`); `mediation/{store,csv,capacity,status,circuit_counter,locations}.h`.
- Modernized: SQLite handles/statements/errors use unique_ptr RAII; inserts use prepared statements; CSV uses string_view and modern parsing. Shared core compiled as a static library.

## inventory-api
- Sources: `inventory-api/main.cpp` plus the same mediation sources except `ingest.cpp`.
- CLI/files: `--db meridian.db`, `--locations data/locations.csv`, `--port 8081`; loads locations once at startup.
- HTTP: GET `/sites` (optional `REGION_CD` or, only if region absent, `STATUS_CD`); GET `/circuits` (optional `CIRCUIT_ID`); GET `/capacity` (optional `requested` Mbps, `market`).
- DB: SELECTs `SITE`, `CIRCUIT`; no table writes (SQLite open can create an empty DB file). Capacity uses CSV, not DB.
- Dependencies: SQLite3, Threads, standard library; vendored `third_party/httplib.h`, mediation headers listed above.
- Modernized: cpp-httplib replaces the hand-rolled server; string_view parsing and prepared filter queries. Global snapshots, fixed JSON buffers and legacy prefix routing remain.

## billing-run
- Sources: `billing/run_billing.cpp` plus `billing/{rating,discounts,accounts,tax,proration,promo,suspension,lines,latefee,invoice}.cpp` (shared billing sources).
- CLI/files: `--accounts billing/accounts` reads `*.rec` key=value files; `--usage data/usage.csv`; optional `--period YYYY-MM`; `--json` emits one JSON object per line instead of the text register. `cmake --build build --target register` selects `2026-07`.
- DB/HTTP: none; does not use `meridian.db`. `data/accounts.csv` is not the account-master input.
- Dependencies: C++ standard library/filesystem, Threads; corresponding billing headers plus `money.h`, `datecalc.h`, `mediation/csv.h`.
- Modernized: filesystem account iteration, string_view APIs, chrono date arithmetic; billing core is a shared static library. Manual JSON, double-based money and linear account lookup remain.

## invoice-api
- Sources: `billing/invoice-api/main.cpp` plus shared billing sources above.
- CLI/files: `--accounts billing/accounts`, `--usage data/usage.csv`, `--port 8082`; loads inputs once at startup.
- HTTP: GET `/invoices`, optional `acct` and `period`; empty results with a nonempty account filter return 404, otherwise empty results return 200.
- DB: none; computes invoices directly from account records and usage.
- Dependencies: shared billing headers above and `third_party/httplib.h`; standard library/filesystem, Threads; no SQLite link.
- Modernized: cpp-httplib server and string_view request handling; legacy response bytes preserved. Global snapshots, hand-built JSON/fixed buffers and double-based money remain.

## ipam
- Sources: `network/ipam.cpp`.
- CLI/files: `--devices network/devices.csv`; prints duplicate addresses, counts per /24, and usage within hard-coded reserved `10.20.250.0/24` of `10.20.0.0/16`.
- DB/HTTP: none. Does not read `network/{reserved-ranges.conf,snmp-polling.conf,routing-tables/,firewall-rules/,dns-zones/}`; these are separate manually maintained artifacts.
- Dependencies: standard library and `mediation/csv.h` only.
- Modernized: from_chars/string_view IPv4 parsing, fixed-width integers, range loops and contains; fixed output formatting retained. Network/reservation remain hard-coded.

## Dashboard pages
- `dashboard/index.html`: GET `http://localhost:8081/sites` and `/circuits`.
- `dashboard/billing.html`: GET `http://localhost:8082/invoices?period=...`, optional `&acct=...`; offers `2026-07` and `2026-06`.
- `dashboard/capacity.html`: GET `http://localhost:8081/capacity` once; requested bandwidth, market and text filtering run client-side (it does not send requested/market parameters).
- Static files; `cmake --build build --target demo` serves dashboard on 8083 with Python http.server and starts invoice-api on 8082, but does not start inventory-api.

## Shared build and verification
- Requires CMake >=3.16, C++20, SQLite3, Threads and GoogleTest. Build/test: `cmake -S . -B build && cmake --build build && ctest --test-dir build`. Binaries are emitted into source-root `bin/`; compilation uses `-O2 -Wall`.
- `billing/rules_test.cpp` uses GoogleTest and is registered with CTest. Inventory/HTTP/IPAM are covered by external baseline captures, not CTest.
- cpp-httplib replaces the removed httpd.h; legacy prefix matching and JSON 404 bodies remain. JSON formatting remains manual. No install/package layer or sanitizer configuration is provided.
