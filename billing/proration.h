#ifndef MERIDIAN_PRORATION_H
#define MERIDIAN_PRORATION_H

#include <string>

double daily_rate(double monthly_fee);
double prorated_plan_charge(double monthly_fee, double prev_monthly_fee, int change_day);

#endif
