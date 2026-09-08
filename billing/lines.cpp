#include "lines.h"
#include "rules_profile.h"

double multi_line_pct(int line_count) {
  return telco_rules::multi_line_pct(line_count);
}

double multi_line_discount(double recurring_charge, int line_count) {
  return telco_rules::multi_line_discount(
      billing_profile(), recurring_charge, line_count);
}
