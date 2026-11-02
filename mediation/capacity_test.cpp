#include <cstdio>
#include "capacity.h"
#include "locations.h"

/* meridian capacity checks: the sales desk numbers come out of capacity-lib
   with the standard maintenance buffer withheld. run by make check. */

static int failures = 0;

static void expect(bool ok, const char *what) {
  printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) failures++;
}

int main() {
  expect(maintenance_buffer_mbps(1000) == 100, "buffer is 10% of a 1000 mbps link");
  expect(available_capacity(1000, 600) == 300, "available = total - allocated - buffer");
  expect(available_capacity(1000, 950) == 0, "available never goes negative");

  std::vector<Location> locs = load_locations("data/locations.csv");
  expect(!locs.empty() && locs[0].loc_cd == "RIV-01", "RIV-01 is the first serviceable location");
  if (!locs.empty()) {
    const Location &riv = locs[0];
    expect(location_buffer_mbps(riv) == 100, "RIV-01 holds 100 mbps for maintenance");
    expect(location_available_mbps(riv) == 300, "RIV-01 has 300 mbps to sell");
    expect(location_can_support(riv, 300), "RIV-01 can take a 300 mbps order");
    expect(!location_can_support(riv, 350), "RIV-01 cannot take a 350 mbps order");
  }
  for (size_t i = 0; i < locs.size(); i++) {
    const Location &l = locs[i];
    int avail = location_available_mbps(l);
    if (avail != l.total_cap_mbps - l.alloc_cap_mbps - l.total_cap_mbps / 10 && avail != 0) {
      printf("FAIL %s available %d\n", l.loc_cd.c_str(), avail);
      failures++;
    }
  }
  if (failures) {
    printf("\n%d capacity check(s) failed\n", failures);
    return 1;
  }
  printf("\nall capacity checks passed\n");
  return 0;
}
