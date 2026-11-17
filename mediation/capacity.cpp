#include "capacity.h"
#include "capacity_core.h"

/* capacity math. delegates to the shared capacity_core rule so meridian and
   vantage quote the same numbers. */

int available_capacity(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps) {
  return (int)capacity_core::available_capacity(total_mbps, allocated_mbps, maintenance_buffer_mbps);
}

double utilization_pct(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps) {
  return capacity_core::utilization_pct(total_mbps, allocated_mbps, maintenance_buffer_mbps);
}

bool can_support(int total_mbps, int allocated_mbps, int requested_mbps, int maintenance_buffer_mbps) {
  return capacity_core::can_support(total_mbps, allocated_mbps, requested_mbps, maintenance_buffer_mbps);
}
