#include "locations.h"
#include "csv.h"
#include "unified_inventory_rules/capacity.h"

/* the sales desk asks one question: how much is left at this location.
   the answer comes from the shared rule in unified-inventory-rules:
   total - allocated - maintenance buffer. capacity reserved for maintenance
   windows is not sellable, so it is withheld before we quote. */

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
    l.maint_buf_mbps = to_int(r["MAINT_BUF_MBPS"]);
    out.push_back(l);
  }
  return out;
}

int location_available_mbps(const Location &loc) {
  return unified_inventory_rules::available_capacity(loc.total_cap_mbps, loc.alloc_cap_mbps,
                                                     loc.maint_buf_mbps);
}

double location_utilization_pct(const Location &loc) {
  return unified_inventory_rules::utilization_pct(loc.total_cap_mbps, loc.alloc_cap_mbps,
                                                  loc.maint_buf_mbps);
}

bool location_can_support(const Location &loc, int requested_mbps) {
  return unified_inventory_rules::can_support(loc.total_cap_mbps, loc.alloc_cap_mbps,
                                              loc.maint_buf_mbps, requested_mbps);
}
