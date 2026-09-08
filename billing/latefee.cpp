#include "latefee.h"
#include "rules_profile.h"

long days_past_due(const std::string &due_date, const std::string &period) {
  return telco_rules::days_past_due(due_date, period);
}

double late_fee(double prior_balance, const std::string &due_date, const std::string &period) {
  return telco_rules::late_fee(
      billing_profile(), prior_balance, due_date, period);
}
