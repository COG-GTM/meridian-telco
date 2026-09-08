#include "locations.h"
#include "capacity.h"
#include "csv.h"
#include <telco_capacity/capacity.h>

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
    l.maint_buffer_mbps = to_int(r["MAINT_BUFFER_MBPS"]);
    out.push_back(l);
  }
  return out;
}

int location_available_mbps(const Location &loc) {
  return available_capacity(loc.total_cap_mbps, loc.alloc_cap_mbps,
                            loc.maint_buffer_mbps);
}

bool location_can_support(const Location &loc, int requested_mbps) {
  return telco_capacity::can_support(location_available_mbps(loc),
                                     requested_mbps);
}
