# ADR 0002: Adopt GoogleTest and CMake

Status: Accepted

## Context

`billing/rules_test.cpp` used a hand-rolled assert harness. It stopped at the
first failure and gave no per-case reporting. The build was a hand-written
Makefile with no dependency discovery, no pinned C++ standard and no test
runner integration.

## Decision

- Convert `billing/rules_test.cpp` to GoogleTest: one `TEST()` per legacy
  check, with the same inputs and expected values. Link it against the system
  `GTest::gtest` / `GTest::gtest_main` (Ubuntu `libgtest-dev`).
- Replace the Makefile with `CMakeLists.txt` (CMake >= 3.16, C++20 required).
  It builds the same binaries into `bin/`, registers tests with
  `gtest_discover_tests()` so `ctest` lists each case, and keeps the `run`,
  `register` and `demo` targets.

## Consequences

- `cmake` and `libgtest-dev` become build prerequisites. CI installs them.
- `make check` becomes `ctest --test-dir build`, and `make register` becomes
  `cmake --build build --target register`.
- Each test case is reported separately and a failure no longer hides later
  checks.

## Alternatives considered

- Catch2 or doctest: also fine. GoogleTest was chosen because the user asked
  for it and it is packaged on the CI image.
- Keep the Makefile and add `-std=c++20`: rejected because it gives no test
  discovery and no portable dependency lookup.
