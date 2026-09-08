#ifndef MERIDIAN_MONEY_H
#define MERIDIAN_MONEY_H

#include "telco_billing_rules.h"

inline double money(double amount) {
  return telco_rules::money(amount);
}

#endif
