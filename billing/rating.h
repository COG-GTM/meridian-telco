#ifndef MERIDIAN_RATING_H
#define MERIDIAN_RATING_H

#include "telco_billing_rules.h"

#define OVERAGE_RATE_PER_GB telco_rules::OVERAGE_RATE_PER_GB

long usage_gb_rounded(long usage_mb);
long overage_gb(long usage_mb, long included_gb);
double rate_overage(long usage_mb, long included_gb);

#endif
