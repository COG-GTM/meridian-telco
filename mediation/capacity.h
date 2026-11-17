#ifndef MERIDIAN_CAPACITY_H
#define MERIDIAN_CAPACITY_H

/* capacity math. the rule lives in third_party/capacity_core/capacity_core.h
   (COG-GTM/telco-capacity-core), shared with vantage-telco:
     available = max(total - allocated - maintenance_buffer, 0)
   maintenance_buffer_mbps defaults to 0 for legacy rows. */

int available_capacity(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps = 0);

/* percent, 2 decimals */
double utilization_pct(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps = 0);

bool can_support(int total_mbps, int allocated_mbps, int requested_mbps,
                 int maintenance_buffer_mbps = 0);

#endif
