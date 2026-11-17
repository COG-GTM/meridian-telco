#include <cstdio>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "capacity.h"
#include "capacity_core_vectors.h"
#include "locations.h"

/* capacity rule checks. bin/capacity-test <test_vectors.json> [locations.csv],
   run with make check. the vectors are the ones vantage-telco is tested
   against, vendored from telco-capacity-core. */

static int failures = 0;

static void check(const std::string &what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what.c_str());
    failures++;
  }
}

int main(int argc, char **argv) {
  const char *vectors = argc > 1 ? argv[1] : "third_party/capacity_core/test_vectors.json";
  const char *locations = argc > 2 ? argv[2] : "data/locations.csv";

  std::vector<capacity_core::Vector> cases = capacity_core::load_vectors(vectors);
  if (cases.empty()) {
    printf("FAIL no capacity vectors in %s\n", vectors);
    return 1;
  }
  for (size_t i = 0; i < cases.size(); i++) {
    const capacity_core::Vector &v = cases[i];
    int t = (int)v.total_mbps, a = (int)v.allocated_mbps, b = (int)v.maintenance_buffer_mbps;
    check(v.name + " available", available_capacity(t, a, b) == v.expect_available_mbps);
    check(v.name + " utilization", fabs(utilization_pct(t, a, b) - v.expect_utilization_pct) < 1e-9);
    check(v.name + " can_support", can_support(t, a, (int)v.requested_mbps, b) == v.expect_can_support);

    Location l;
    l.total_cap_mbps = t;
    l.alloc_cap_mbps = a;
    l.maint_buffer_mbps = b;
    check(v.name + " location_available_mbps", location_available_mbps(l) == v.expect_available_mbps);
    check(v.name + " location_can_support",
          location_can_support(l, (int)v.requested_mbps) == v.expect_can_support);
  }
  printf("ok   %d/%d shared capacity vectors\n", (int)cases.size() - failures, (int)cases.size());

  /* buffer defaults to 0: the old two argument answer still holds */
  check("buffer defaults to 0", available_capacity(1000, 600) == 400 && utilization_pct(1000, 600) == 60.0);

  /* legacy locations drop without MAINT_BUFFER_MBPS loads a 0 buffer */
  const char *legacy = "capacity_test_legacy.csv";
  {
    std::ofstream f(legacy);
    f << "LOC_CD,CUST_NM,LOC_NM,MARKET_CD,TOTAL_CAP_MBPS,ALLOC_CAP_MBPS\n";
    f << "LEG-01,Legacy Co,Legacy Site,BOS-12,1000,600\n";
  }
  std::vector<Location> old_rows = load_locations(legacy);
  remove(legacy);
  check("legacy locations row loads", old_rows.size() == 1);
  if (old_rows.size() == 1) {
    check("legacy row buffer is 0", old_rows[0].maint_buffer_mbps == 0);
    check("legacy row available is total - allocated", location_available_mbps(old_rows[0]) == 400);
  }

  /* the seeded locations carry their buffer */
  std::vector<Location> locs = load_locations(locations);
  bool found = false;
  for (size_t i = 0; i < locs.size(); i++) {
    if (locs[i].loc_cd != "RIV-01") continue;
    found = true;
    check("RIV-01 buffer is 150", locs[i].maint_buffer_mbps == 150);
    check("RIV-01 available is 1000 - 600 - 150", location_available_mbps(locs[i]) == 250);
    check("RIV-01 utilization is 75.00", location_utilization_pct(locs[i]) == 75.0);
    check("RIV-01 can support 250", location_can_support(locs[i], 250));
    check("RIV-01 cannot support 251", !location_can_support(locs[i], 251));
  }
  check("RIV-01 in " + std::string(locations), found);

  if (failures) {
    printf("%d capacity check(s) failed\n", failures);
    return 1;
  }
  printf("all capacity checks passed\n");
  return 0;
}
