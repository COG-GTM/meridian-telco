# Java 21 modernization — progress

Integration branch: `java21-modernization`. Legacy C++ stays the fallback until Phase 5.

| Phase | Track | Status | Notes |
|---|---|---|---|
| P0 | Contract freeze (`contracts/`, `tools/legacy/capture-contracts.sh`) | done | /sites /circuits /capacity /invoices, register text + `--json` (400 invoices), mediation, ipam, billing-test |
| P0 | Maven multi-module, Spring Boot 3.5, Java 21, CI | done | `.github/workflows/build.yml`: `mvn verify` + legacy `--check` |
| P0 | Golden/characterization tests | written, tagged `pending` | 22 billing rule checks, register byte-diff, CSV parser, capacity, HTTP contracts, ipam |
| P1 | Flyway schemas per service | done | inventory / billing / network PostgreSQL |
| P1 | `db-migration` (SQLite + `.rec` + network conf → Postgres) | done | verified against Postgres 16 (Testcontainers + real `bin/mediation` meridian.db via `ops/compose.yaml`): 412 sites, 240 circuits, 200 accounts, 147 devices, 2 reserved ranges, 138 SNMP targets |
| P2 | inventory-service :8081 | merged (PR #33) | [child A](https://devin-gtm.devinenterprise.com/sessions/f4b1ae5756bc4aa0a29a944849a4feee); endpoints + mediation report byte-identical; Kafka producer; 83 tests |
| P3 | billing-service :8082 + billing-run | in progress | [child B](https://devin-gtm.devinenterprise.com/sessions/b9447085405848858ad5da7045b8cb4c) |
| P4 | network-service + dashboard | merged (PR #32) | [child C](https://devin-gtm.devinenterprise.com/sessions/555c095c0d0b4ada9b11551f201cb284); :8084, `/ipam/report.txt` byte-identical; 21 tests |
| P5 | parity diff, decommission | pending | |
