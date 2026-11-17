#ifndef TELCO_CAPACITY_CORE_H
#define TELCO_CAPACITY_CORE_H

/* Canonical capacity / maintenance-buffer rule shared by Vantage and Meridian.
   Header-only mirror of the Python package capacity_core; both are checked
   against test_vectors.json.

     available       = max(total - allocated - maintenance_buffer, 0)
     utilization_pct = (allocated + maintenance_buffer) * 100 / total   (0 when total <= 0)
     can_support     = requested <= available

   utilization is integer math, rounded to 2 decimals half to even. */

#define CAPACITY_CORE_VERSION "1.0.0"
#define CAPACITY_CORE_RULE "available = total - allocated - maintenance_buffer"

namespace capacity_core {

inline long long available_capacity(long long total_mbps, long long allocated_mbps,
                                    long long maintenance_buffer_mbps = 0) {
  long long avail = total_mbps - allocated_mbps - maintenance_buffer_mbps;
  return avail < 0 ? 0 : avail;
}

/* utilization in hundredths of a percent (7500 == 75.00%) */
inline long long utilization_centi_pct(long long total_mbps, long long allocated_mbps,
                                       long long maintenance_buffer_mbps = 0) {
  if (total_mbps <= 0) return 0;
  long long num = (allocated_mbps + maintenance_buffer_mbps) * 10000;
  long long q = num / total_mbps;
  long long r = num % total_mbps;
  if (r < 0) { q -= 1; r += total_mbps; } /* floor division, as Python divmod */
  long long twice = 2 * r;
  if (twice > total_mbps || (twice == total_mbps && (q % 2 != 0))) q += 1;
  return q;
}

inline double utilization_pct(long long total_mbps, long long allocated_mbps,
                              long long maintenance_buffer_mbps = 0) {
  return (double)utilization_centi_pct(total_mbps, allocated_mbps, maintenance_buffer_mbps) / 100.0;
}

inline bool can_support(long long total_mbps, long long allocated_mbps, long long requested_mbps,
                        long long maintenance_buffer_mbps = 0) {
  return requested_mbps <= available_capacity(total_mbps, allocated_mbps, maintenance_buffer_mbps);
}

}  // namespace capacity_core

#endif
