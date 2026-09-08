#ifndef MERIDIAN_MONEY_H
#define MERIDIAN_MONEY_H

#include "telco_rules/billing.h"

inline double money(double amount) {
  return telco_rules::money(amount);
}

#endif
