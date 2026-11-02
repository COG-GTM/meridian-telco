#ifndef MERIDIAN_CAPACITY_H
#define MERIDIAN_CAPACITY_H

/* capacity arithmetic comes from the shared capacity-lib (vendor/capacity-lib),
   the same rule vantage-telco uses:
     BUFFER_CAP_MBPS = 10% of TOTAL_CAP_MBPS
     AVAIL_CAP_MBPS  = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS - BUFFER_CAP_MBPS */

#define CAPACITY_RULE \
  "AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS - BUFFER_CAP_MBPS; BUFFER_CAP_MBPS = 10% of TOTAL_CAP_MBPS"

int maintenance_buffer_mbps(int total_mbps);
int available_capacity(int total_mbps, int allocated_mbps);
int utilization_pct(int total_mbps, int allocated_mbps);

#endif
