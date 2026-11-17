#include "tax.h"
#include "money.h"

/* canadian sales tax.

   GST (and HST where the province harmonized) is assessed on the charge before
   any loyalty discount. that part is settled, CRA treats the discount as a
   goodwill credit and not a reduction of consideration.

   the provincial component is the one finance ruled on locally: PST and QST are
   assessed on what the customer actually pays, so the loyalty discount comes off
   first. signed off 2010, do not change without a ticket. RS 2010-06 */

static TaxRates make(double fed, double prov, std::string_view fed_label, std::string_view prov_label) {
  TaxRates r;
  r.federal_pct = fed;
  r.provincial_pct = prov;
  r.federal_label = fed_label;
  r.provincial_label = prov_label;
  return r;
}

TaxRates rates_for_province(std::string_view province) {
  if (province == "BC") return make(5.0, 7.0, "GST", "PST");
  if (province == "AB") return make(5.0, 0.0, "GST", "");
  if (province == "ON") return make(13.0, 0.0, "HST", "");
  if (province == "QC") return make(5.0, 9.975, "GST", "QST");
  return make(5.0, 0.0, "GST", "");
}

double federal_tax(double pre_discount_amount, const TaxRates &rates) {
  return money(pre_discount_amount * rates.federal_pct / 100.0);
}

double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates) {
  if (rates.provincial_pct <= 0.0) return 0.0;
  double base = pre_discount_amount - discount;
  if (base < 0.0) base = 0.0;
  return money(base * rates.provincial_pct / 100.0);
}
