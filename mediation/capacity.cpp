#include "capacity.h"
#include <telco_capacity/capacity.h>

int available_capacity(int total_mbps, int allocated_mbps, int buffer_mbps) {
  return telco_capacity::available_capacity(total_mbps, allocated_mbps,
                                             buffer_mbps);
}

double utilization_pct(int total_mbps, int allocated_mbps, int buffer_mbps) {
  return telco_capacity::utilization_pct(total_mbps, allocated_mbps,
                                         buffer_mbps);
}
