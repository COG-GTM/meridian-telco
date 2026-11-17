cmake_minimum_required(VERSION 3.16)
project(meridian_telco LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
add_compile_options(-O2 -Wall)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/third_party")

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_library(mediation_core STATIC
  mediation/capacity.cpp
  mediation/status.cpp
  mediation/circuit_counter.cpp
  mediation/store.cpp
  mediation/locations.cpp
)
target_link_libraries(mediation_core PUBLIC SQLite::SQLite3 Threads::Threads)

add_library(billing_core STATIC
  billing/rating.cpp
  billing/discounts.cpp
  billing/accounts.cpp
  billing/tax.cpp
  billing/proration.cpp
  billing/promo.cpp
  billing/suspension.cpp
  billing/lines.cpp
  billing/latefee.cpp
  billing/invoice.cpp
)
target_link_libraries(billing_core PUBLIC Threads::Threads)

add_executable(mediation mediation/ingest.cpp)
target_link_libraries(mediation PRIVATE mediation_core)
add_executable(inventory-api inventory-api/main.cpp)
target_link_libraries(inventory-api PRIVATE mediation_core)
add_executable(billing-run billing/run_billing.cpp)
target_link_libraries(billing-run PRIVATE billing_core)
add_executable(invoice-api billing/invoice-api/main.cpp)
target_link_libraries(invoice-api PRIVATE billing_core)
add_executable(ipam network/ipam.cpp)
target_link_libraries(ipam PRIVATE Threads::Threads)

add_executable(billing-test billing/rules_test.cpp)
target_link_libraries(billing-test PRIVATE billing_core GTest::gtest GTest::gtest_main)
enable_testing()
add_test(NAME billing-test COMMAND billing-test)
set_tests_properties(billing-test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set(main_binaries mediation inventory-api billing-run invoice-api ipam)
add_custom_target(run
  COMMAND $<TARGET_FILE:mediation> --data data --db meridian.db
  DEPENDS ${main_binaries}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
)
add_custom_target(register
  COMMAND $<TARGET_FILE:billing-run> --period 2026-07
  DEPENDS ${main_binaries}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
)
add_custom_target(demo
  COMMAND ${CMAKE_COMMAND} -E echo "invoice-api on :8082, invoice register page on :8083"
  COMMAND /bin/sh -c "\"$<TARGET_FILE:invoice-api>\" & python3 -m http.server 8083 --directory dashboard & wait"
  DEPENDS ${main_binaries}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  USES_TERMINAL
  VERBATIM
)
