#include "capacity.h"
#include "telco_capacity/capacity.h"

int available_capacity(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps) {
  return telco_capacity::available_capacity(total_mbps, allocated_mbps, maintenance_buffer_mbps);
}

double utilization_pct(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps) {
  return telco_capacity::utilization_pct(total_mbps, allocated_mbps, maintenance_buffer_mbps);
}

const char *capacity_rule() { return telco_capacity::RULE; }
