#ifndef MERIDIAN_TAX_H
#define MERIDIAN_TAX_H

#include <string>
#include <string_view>

struct TaxRates {
  double federal_pct;    /* GST, or HST where the province is harmonized */
  double provincial_pct; /* PST or QST, zero in a harmonized province */
  std::string federal_label;
  std::string provincial_label;
};

TaxRates rates_for_province(std::string_view province);

double federal_tax(double pre_discount_amount, const TaxRates &rates);
double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates);

#endif
