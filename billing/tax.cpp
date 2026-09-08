#include "tax.h"
#include "rules_profile.h"

TaxRates rates_for_province(const std::string &province) {
  return telco_rules::rates_for_province(province);
}

double federal_tax(double pre_discount_amount, const TaxRates &rates) {
  return telco_rules::federal_tax(
      billing_profile(), pre_discount_amount, rates);
}

double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates) {
  return telco_rules::provincial_tax(
      billing_profile(), pre_discount_amount, discount, rates);
}
