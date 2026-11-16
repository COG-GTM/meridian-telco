#ifndef MERIDIAN_CAPACITY_H
#define MERIDIAN_CAPACITY_H

/* capacity math lives in the shared telco-capacity-rules repo
   (third_party/telco-capacity-rules, see SPEC.md there). these delegate to it.

   AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS - MAINT_BUFFER_MBPS, floor 0
   the maintenance buffer is reserved capacity and is never sellable. */

int available_capacity(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps = 0);

/* (allocated + buffer) * 100 / total, half-up to 2 decimals, 0 when total <= 0 */
double utilization_pct(int total_mbps, int allocated_mbps, int maintenance_buffer_mbps = 0);

/* "AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS - MAINT_BUFFER_MBPS" */
const char *capacity_rule();

#endif
