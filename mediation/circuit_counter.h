#ifndef MERIDIAN_CIRCUIT_COUNTER_H
#define MERIDIAN_CIRCUIT_COUNTER_H

#include <string>
#include <vector>
#include "csv.h"

inline constexpr int ROLE_PRIMARY = 1;
inline constexpr int ROLE_STANDBY = 2;
inline constexpr int ROLE_FAILOVER = 3;

int count_active_circuits(const std::vector<Row> &circuits);
int sum_active_capacity(const std::vector<Row> &circuits);

#endif
