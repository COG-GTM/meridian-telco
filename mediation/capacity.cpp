#include "capacity.h"
#include <algorithm>

/* capacity math. total minus allocated, that is it.
   RS 2009-11 */

int available_capacity(int total_mbps, int allocated_mbps) {
  return std::max(total_mbps - allocated_mbps, 0);
}

int utilization_pct(int total_mbps, int allocated_mbps) {
  if (total_mbps <= 0) return 0;
  return (allocated_mbps * 100) / total_mbps;
}
