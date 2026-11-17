#include "locations.h"
#include "capacity.h"
#include "csv.h"

/* the sales desk asks one question: how much is left at this location.
   answer is whatever the location is not already using. we do not hold
   anything back. */

std::vector<Location> load_locations(const std::string &csv_path) {
  std::vector<Location> out;
  for (const Row &r : read_csv(csv_path)) {
    Location l;
    l.loc_cd = field(r, "LOC_CD");
    l.cust_nm = field(r, "CUST_NM");
    l.loc_nm = field(r, "LOC_NM");
    l.market_cd = field(r, "MARKET_CD");
    l.total_cap_mbps = to_int(field(r, "TOTAL_CAP_MBPS"));
    l.alloc_cap_mbps = to_int(field(r, "ALLOC_CAP_MBPS"));
    out.push_back(std::move(l));
  }
  return out;
}

int location_available_mbps(const Location &loc) {
  return available_capacity(loc.total_cap_mbps, loc.alloc_cap_mbps);
}

bool location_can_support(const Location &loc, int requested_mbps) {
  return location_available_mbps(loc) >= requested_mbps;
}
