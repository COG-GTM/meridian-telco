#ifndef TELCO_BILLING_RULES_H
#define TELCO_BILLING_RULES_H

// constants mirror telco_billing_rules/billing_rules.json; tests/test_rules.py
// fails if they drift.

#include <cmath>
#include <string>

namespace telco_rules {

constexpr int BILLING_MONTH_DAYS = 30;
constexpr long MB_PER_GB = 1024;
constexpr double OVERAGE_RATE_PER_GB = 10.00;

/* every charge line is rounded to cents as it is produced. the register has
   always been assembled from rounded lines, the total is just their sum.
   RS 2011-03 */
inline double money(double amount) {
  return floor(amount * 100.0 + 0.5) / 100.0;
}

/* proration. the billing month is 30 days, always, whatever the calendar says.
   this is the convention the register has used since the first cycle and the
   downstream reconciliation reports assume it. do not switch to actual days
   without a ticket. RS 2010-02 */
inline double daily_rate(double monthly_fee) {
  return monthly_fee / (double)BILLING_MONTH_DAYS;
}

inline double prorated_plan_charge(double monthly_fee, double prev_monthly_fee, int change_day) {
  if (change_day <= 0) return money(monthly_fee);
  int days_on_old = change_day - 1;
  if (days_on_old < 0) days_on_old = 0;
  if (days_on_old > BILLING_MONTH_DAYS) days_on_old = BILLING_MONTH_DAYS;
  int days_on_new = BILLING_MONTH_DAYS - days_on_old;
  double old_part = money(daily_rate(prev_monthly_fee) * (double)days_on_old);
  double new_part = money(daily_rate(monthly_fee) * (double)days_on_new);
  return money(old_part + new_part);
}

/* rating engine. usage comes off the mediation drop in megabytes.
   billing has always been done in whole gigabytes, partial gigs round up.
   $10 a gig over the plan allowance, flat, no tiers. */
inline long usage_gb_rounded(long usage_mb) {
  long gb = usage_mb / MB_PER_GB;
  if (usage_mb % MB_PER_GB) gb++;
  return gb;
}

inline long overage_gb(long usage_mb, long included_gb) {
  long gb = usage_gb_rounded(usage_mb);
  if (gb <= included_gb) return 0;
  return gb - included_gb;
}

inline double rate_overage(long usage_mb, long included_gb) {
  return money((double)overage_gb(usage_mb, included_gb) * OVERAGE_RATE_PER_GB);
}

struct TaxRates {
  double federal_pct;
  double provincial_pct;
  std::string federal_label;
  std::string provincial_label;
};

inline TaxRates make(double fed, double prov, const char *fed_label, const char *prov_label) {
  TaxRates r;
  r.federal_pct = fed;
  r.provincial_pct = prov;
  r.federal_label = fed_label;
  r.provincial_label = prov_label;
  return r;
}

/* canadian sales tax.

   GST (and HST where the province harmonized) is assessed on the charge before
   any loyalty discount. that part is settled, CRA treats the discount as a
   goodwill credit and not a reduction of consideration.

   the provincial component is the one finance ruled on locally: PST and QST are
   assessed on what the customer actually pays, so the loyalty discount comes off
   first. signed off 2010, do not change without a ticket. RS 2010-06 */
inline TaxRates rates_for_province(const std::string &province) {
  if (province == "BC") return make(5.0, 7.0, "GST", "PST");
  if (province == "AB") return make(5.0, 0.0, "GST", "");
  if (province == "ON") return make(13.0, 0.0, "HST", "");
  if (province == "QC") return make(5.0, 9.975, "GST", "QST");
  return make(5.0, 0.0, "GST", "");
}

inline double federal_tax(double pre_discount_amount, const TaxRates &rates) {
  return money(pre_discount_amount * rates.federal_pct / 100.0);
}

inline double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates) {
  if (rates.provincial_pct <= 0.0) return 0.0;
  double base = pre_discount_amount - discount;
  if (base < 0.0) base = 0.0;
  return money(base * rates.provincial_pct / 100.0);
}

}  // namespace telco_rules

#endif
