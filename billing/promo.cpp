#include "promo.h"
#include "rules_profile.h"

bool promo_is_live(const std::string &issued_on, const std::string &period) {
  return telco_rules::promo_is_live(billing_profile(), issued_on, period);
}

double promo_credit(double amount, const std::string &issued_on, const std::string &period) {
  return telco_rules::promo_credit(
      billing_profile(), amount, issued_on, period);
}
