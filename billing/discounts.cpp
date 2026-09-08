#include "discounts.h"
#include "money.h"
#include "rules_profile.h"

double loyalty_discount(double amount, double loyalty_pct) {
  return telco_rules::loyalty_discount(
      billing_profile(), amount, loyalty_pct);
}

double apply_loyalty(double amount, double loyalty_pct) {
  return money(amount - loyalty_discount(amount, loyalty_pct));
}
