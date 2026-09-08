#ifndef MERIDIAN_TAX_H
#define MERIDIAN_TAX_H

#include "telco_rules/billing.h"

using TaxRates = telco_rules::TaxRates;

TaxRates rates_for_province(const std::string &province);

double federal_tax(double pre_discount_amount, const TaxRates &rates);
double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates);

#endif
