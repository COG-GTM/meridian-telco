#include "locations.h"
#include <capacity_lib/capacity.h>
#include "csv.h"

/* the sales desk asks one question: how much is left at this location.
   answer comes from the shared capacity-lib: what the location is not
   already using, less the standard maintenance buffer. */

std::vector<Location> load_locations(const std::string &csv_path) {
  std::vector<Location> out;
  std::vector<Row> rows = read_csv(csv_path);
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    Location l;
    l.loc_cd = r["LOC_CD"];
    l.cust_nm = r["CUST_NM"];
    l.loc_nm = r["LOC_NM"];
    l.market_cd = r["MARKET_CD"];
    l.total_cap_mbps = to_int(r["TOTAL_CAP_MBPS"]);
    l.alloc_cap_mbps = to_int(r["ALLOC_CAP_MBPS"]);
    out.push_back(l);
  }
  return out;
}

int location_buffer_mbps(const Location &loc) {
  return capacity_lib::maintenance_buffer_mbps(loc.total_cap_mbps);
}

int location_available_mbps(const Location &loc) {
  return capacity_lib::available_capacity(loc.total_cap_mbps, loc.alloc_cap_mbps,
                                          location_buffer_mbps(loc));
}

bool location_can_support(const Location &loc, int requested_mbps) {
  return capacity_lib::can_support(loc.total_cap_mbps, loc.alloc_cap_mbps, requested_mbps);
}
