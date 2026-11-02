#ifndef MERIDIAN_LOCATIONS_H
#define MERIDIAN_LOCATIONS_H

#include <string>
#include <vector>

/* serviceable customer locations, used by the sales capacity check */

struct Location {
  std::string loc_cd;
  std::string cust_nm;
  std::string loc_nm;
  std::string market_cd;
  int total_cap_mbps;
  int alloc_cap_mbps;
};

std::vector<Location> load_locations(const std::string &csv_path);

/* mbps held back for maintenance (capacity-lib standard buffer) */
int location_buffer_mbps(const Location &loc);

/* mbps that can still be sold at this location, after the maintenance buffer */
int location_available_mbps(const Location &loc);

/* can we sell the requested bandwidth here */
bool location_can_support(const Location &loc, int requested_mbps);

#endif
