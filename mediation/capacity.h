#ifndef MERIDIAN_CAPACITY_H
#define MERIDIAN_CAPACITY_H

[[nodiscard]] int available_capacity(int total_mbps, int allocated_mbps);
[[nodiscard]] int utilization_pct(int total_mbps, int allocated_mbps);

#endif
