#ifndef MERIDIAN_CIRCUIT_COUNTER_H
#define MERIDIAN_CIRCUIT_COUNTER_H

#include <string>
#include <vector>
#include "csv.h"
#include "telco_rules/circuits.h"

using telco_rules::ROLE_PRIMARY;
using telco_rules::ROLE_STANDBY;
using telco_rules::ROLE_FAILOVER;

int count_active_circuits(const std::vector<Row> &circuits);
int sum_active_capacity(const std::vector<Row> &circuits);

#endif
