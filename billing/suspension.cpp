#include "suspension.h"
#include "rules_profile.h"

int suspended_days(int start_day, int end_day) {
  return telco_rules::suspended_days(start_day, end_day);
}

double suspension_credit(double monthly_fee, int start_day, int end_day) {
  return telco_rules::suspension_credit(
      billing_profile(), monthly_fee, start_day, end_day, "");
}
