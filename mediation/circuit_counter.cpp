#include "circuit_counter.h"

/* circuit counts for the nightly inventory roll-up */

int count_active_circuits(const std::vector<Row> &circuits) {
  int n = 0;
  for (size_t i = 0; i < circuits.size(); i++) {
    Row r = circuits[i];
    /* standby circuits count, DECISIONS.md D-08 */
    if (!telco_rules::circuit_is_active(to_int(r["STATUS_CD"]), to_int(r["ROLE_CD"]))) continue;
    n++;
  }
  return n;
}

int sum_active_capacity(const std::vector<Row> &circuits) {
  int total = 0;
  for (size_t i = 0; i < circuits.size(); i++) {
    Row r = circuits[i];
    if (!telco_rules::circuit_is_active(to_int(r["STATUS_CD"]), to_int(r["ROLE_CD"]))) continue;
    total += to_int(r["CAP_MBPS"]);
  }
  return total;
}
