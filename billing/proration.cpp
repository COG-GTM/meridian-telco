#include "proration.h"
#include "money.h"

double daily_rate(double monthly_fee) {
  return telco_rules::daily_rate(monthly_fee);
}

double prorated_plan_charge(double monthly_fee, double prev_monthly_fee, int change_day) {
  return telco_rules::prorated_plan_charge(monthly_fee, prev_monthly_fee, change_day);
}
