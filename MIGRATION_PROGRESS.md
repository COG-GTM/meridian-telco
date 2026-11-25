# Java 21 modernization — progress

Integration branch: `java21-modernization`. Legacy C++ is removed in the P5 decommission PR; the frozen `contracts/` (captured from the C++ binaries) remain the parity baseline.

| Phase | Track | Status | Notes |
|---|---|---|---|
| P0 | Contract freeze (`contracts/`, captured by the since-removed `tools/legacy/capture-contracts.sh`) | done | /sites /circuits /capacity /invoices, register text + `--json` (400 invoices), mediation, ipam, billing-test |
| P0 | Maven multi-module, Spring Boot 3.5, Java 21, CI | done | `.github/workflows/build.yml`: `mvn verify` + packaged billing-run vs frozen registers (legacy `--check` and C++ register-parity jobs removed with the C++ sources) |
| P0 | Golden/characterization tests | active (all `pending` tags removed) | 22 billing rule checks, register byte-diff, CSV parser, capacity, HTTP contracts, ipam |
| P1 | Flyway schemas per service | done | inventory / billing / network PostgreSQL |
| P1 | `db-migration` (SQLite + `.rec` + network conf → Postgres) | done | verified against Postgres 16 (Testcontainers + real `bin/mediation` meridian.db via `ops/compose.yaml`): 412 sites, 240 circuits, 200 accounts, 147 devices, 2 reserved ranges, 138 SNMP targets |
| P2 | inventory-service :8081 | merged (PR #33) | [child A](https://devin-gtm.devinenterprise.com/sessions/f4b1ae5756bc4aa0a29a944849a4feee); endpoints + mediation report byte-identical; Kafka producer; 83 tests |
| P3 | billing-service :8082 + billing-run | merged (PR #34) | [child B](https://devin-gtm.devinenterprise.com/sessions/b9447085405848858ad5da7045b8cb4c); register text + `--json` byte-identical for 2026-06/07 ; 59 tests |
| P4 | network-service + dashboard | merged (PR #32) | [child C](https://devin-gtm.devinenterprise.com/sessions/555c095c0d0b4ada9b11551f201cb284); :8084, `/ipam/report.txt` byte-identical; 21 tests |
| P5 | parity diff, decommission | done on fixtures; merge held | Java matches C++ byte-for-byte on 2026-06/07 and all frozen HTTP contracts. Merge only after a real billing cycle is diffed old-vs-new |
