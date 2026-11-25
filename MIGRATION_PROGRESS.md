# Java 21 modernization — progress

Integration branch: `java21-modernization`. Legacy C++ stays the fallback until Phase 5.

| Phase | Track | Status | Notes |
|---|---|---|---|
| P0 | Contract freeze (`contracts/`, `tools/legacy/capture-contracts.sh`) | done | /sites /circuits /capacity /invoices, register text + `--json` (400 invoices), mediation, ipam, billing-test |
| P0 | Maven multi-module, Spring Boot 3.5, Java 21, CI | done | `.github/workflows/build.yml`: `mvn verify` + legacy `--check` |
| P0 | Golden/characterization tests | written, tagged `pending` | 22 billing rule checks, register byte-diff, CSV parser, capacity, HTTP contracts, ipam |
| P1 | Flyway schemas per service | done | inventory / billing / network PostgreSQL |
| P1 | `db-migration` (SQLite + `.rec` + network conf → Postgres) | done | verified against Postgres 16 (Testcontainers): 412 sites, 240 circuits, 200 accounts, 147 devices |
| P2 | inventory-service :8081 | pending | |
| P3 | billing-service :8082 + billing-run | pending | |
| P4 | network-service + dashboard | pending | |
| P5 | parity diff, decommission | pending | |
