#include "promo.h"
#include "datecalc.h"
#include "money.h"

/* promotional credits expire at the end of the billing cycle they were issued
   in. a credit issued in june is a june credit, it does not roll into july.
   care wanted 30 days once, finance said no, the cycle is the unit. RS 2012-08 */

bool promo_is_live(std::string_view issued_on, std::string_view period) {
  BillDate issued = parse_date(issued_on);
  BillDate cycle = period_start(period);
  if (!issued.ok || !cycle.ok) return false;
  return issued.y == cycle.y && issued.m == cycle.m;
}

double promo_credit(double amount, std::string_view issued_on, std::string_view period) {
  if (amount <= 0.0) return 0.0;
  if (!promo_is_live(issued_on, period)) return 0.0;
  return money(amount);
}
