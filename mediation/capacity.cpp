#include "capacity.h"
#include <capacity_lib/capacity.h>

int maintenance_buffer_mbps(int total_mbps) {
  return capacity_lib::maintenance_buffer_mbps(total_mbps);
}

int available_capacity(int total_mbps, int allocated_mbps) {
  return capacity_lib::available_capacity(total_mbps, allocated_mbps,
                                          capacity_lib::maintenance_buffer_mbps(total_mbps));
}

int utilization_pct(int total_mbps, int allocated_mbps) {
  if (total_mbps <= 0) return 0;
  return (allocated_mbps * 100) / total_mbps;
}
