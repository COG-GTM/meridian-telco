#include "billing_rules/rules.h"

#include <cmath>

#include "billing_rules/datecalc.h"

namespace billing_rules {

/* money ------------------------------------------------------------------ */

double money(double amount) {
  /* Half up to cents. A product such as 1102.50 * 0.13 lands a hair below the
     exact 143.325 in binary, so a bare floor(x * 100 + 0.5) rounds a genuine
     half down; the tolerance pulls those back onto the half the decimal
     arithmetic in the Python binding sees. */
  double cents = amount * 100.0;
  double tolerance = 1e-9 * fabs(cents);
  if (cents < 0.0) return -floor(-cents + 0.5 + tolerance) / 100.0;
  return floor(cents + 0.5 + tolerance) / 100.0;
}

bool rounds_per_line() {
  return BR_LINE_ROUNDING_PER_LINE != 0;
}

static double line(double amount) {
  return rounds_per_line() ? money(amount) : amount;
}

/* proration -------------------------------------------------------------- */

int billing_days_in_period(const std::string &period) {
  if (BR_PRORATION_CALENDAR_MONTH == 0) return BR_BILLING_MONTH_DAYS;
  BillDate cycle = period_start(period);
  if (!cycle.ok) return BR_BILLING_MONTH_DAYS;
  return calendar_days_in_month(cycle.y, cycle.m);
}

double daily_rate(double monthly_fee, const std::string &period) {
  return monthly_fee / (double)billing_days_in_period(period);
}

double prorated_plan_charge(double monthly_fee, double previous_monthly_fee, int change_day,
                            const std::string &period) {
  if (change_day <= 0) return line(monthly_fee);
  int total_days = billing_days_in_period(period);
  int days_on_old = change_day - 1;
  if (days_on_old < 0) days_on_old = 0;
  if (days_on_old > total_days) days_on_old = total_days;
  int days_on_new = total_days - days_on_old;
  double old_part = line(daily_rate(previous_monthly_fee, period) * (double)days_on_old);
  double new_part = line(daily_rate(monthly_fee, period) * (double)days_on_new);
  return line(old_part + new_part);
}

/* rating ----------------------------------------------------------------- */

long usage_gb_rounded(long usage_mb) {
  long gb = usage_mb / BR_MB_PER_GB;
  if (usage_mb % BR_MB_PER_GB) gb++;
  return gb;
}

long overage_gb(long usage_mb, long included_gb) {
  long gb = usage_gb_rounded(usage_mb);
  if (gb <= included_gb) return 0;
  return gb - included_gb;
}

long overage_mb(long usage_mb, long included_gb) {
  long included_mb = included_gb * BR_MB_PER_GB;
  long over = usage_mb - included_mb;
  return over > 0 ? over : 0;
}

double rate_overage(long usage_mb, long included_gb) {
  if (BR_RATING_EXACT_MB) {
    return line((double)overage_mb(usage_mb, included_gb) * BR_OVERAGE_RATE_PER_MB);
  }
  return line((double)overage_gb(usage_mb, included_gb) * BR_OVERAGE_RATE_PER_GB);
}

/* promo ------------------------------------------------------------------ */

bool promo_is_live(const std::string &issued_on, const std::string &period) {
  BillDate issued = parse_date(issued_on);
  BillDate cycle = period_start(period);
  if (!issued.ok || !cycle.ok) return false;
  if (BR_PROMO_EXPIRY_ROLLING_DAYS == 0) {
    return issued.y == cycle.y && issued.m == cycle.m;
  }
  return serial_day(issued) + BR_PROMO_VALID_DAYS >= serial_day(cycle);
}

double promo_credit(double amount, const std::string &issued_on, const std::string &period) {
  if (amount <= 0.0) return 0.0;
  if (!promo_is_live(issued_on, period)) return 0.0;
  return line(amount);
}

/* suspension ------------------------------------------------------------- */

int suspended_days(int start_day, int end_day) {
  if (start_day <= 0 || end_day < start_day) return 0;
  return end_day - start_day + 1;
}

double suspension_credit(double monthly_fee, int start_day, int end_day, const std::string &period) {
  if (BR_SUSPENSION_CREDIT_DAILY_RATE == 0) return 0.0;
  int days = suspended_days(start_day, end_day);
  if (!days) return 0.0;
  return line(daily_rate(monthly_fee, period) * (double)days);
}

/* multi line ------------------------------------------------------------- */

double multi_line_pct(int line_count) {
  for (int i = 0; i < BR_LINE_TIER_COUNT; i++) {
    if (line_count >= BR_LINE_TIER_MIN[i]) return BR_LINE_TIER_PCT[i];
  }
  return 0.0;
}

double multi_line_discount(double recurring_charge, int line_count) {
  return line(recurring_charge * multi_line_pct(line_count) / 100.0);
}

/* late fee --------------------------------------------------------------- */

long days_past_due(const std::string &due_date, const std::string &period) {
  BillDate due = parse_date(due_date);
  BillDate cycle = period_start(period);
  if (!due.ok || !cycle.ok) return 0;
  long days = days_between(due, cycle);
  return days > 0 ? days : 0;
}

double late_fee(double prior_balance, const std::string &due_date, const std::string &period) {
  if (prior_balance <= 0.0) return 0.0;
  if (days_past_due(due_date, period) <= BR_LATE_FEE_GRACE_DAYS) return 0.0;
  return line(prior_balance * BR_LATE_FEE_PCT / 100.0);
}

/* tax -------------------------------------------------------------------- */

static std::string upper(const std::string &s) {
  std::string out = s;
  for (size_t i = 0; i < out.size(); i++) {
    if (out[i] >= 'a' && out[i] <= 'z') out[i] = (char)(out[i] - 'a' + 'A');
  }
  return out;
}

TaxRates rates_for_province(const std::string &province) {
  std::string key = upper(province);
  TaxRates r;
  for (int i = 0; i < BR_PROVINCE_COUNT; i++) {
    if (key == BR_PROVINCE_CODE[i]) {
      r.federal_pct = BR_PROVINCE_FEDERAL_PCT[i];
      r.provincial_pct = BR_PROVINCE_PROVINCIAL_PCT[i];
      r.federal_label = BR_PROVINCE_FEDERAL_LABEL[i];
      r.provincial_label = BR_PROVINCE_PROVINCIAL_LABEL[i];
      return r;
    }
  }
  r.federal_pct = BR_DEFAULT_FEDERAL_PCT;
  r.provincial_pct = BR_DEFAULT_PROVINCIAL_PCT;
  r.federal_label = BR_DEFAULT_FEDERAL_LABEL;
  r.provincial_label = BR_DEFAULT_PROVINCIAL_LABEL;
  return r;
}

double federal_tax(double pre_discount_amount, const TaxRates &rates) {
  return line(pre_discount_amount * rates.federal_pct / 100.0);
}

double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates) {
  if (rates.provincial_pct <= 0.0) return 0.0;
  double base = pre_discount_amount;
  if (BR_PROVINCIAL_TAX_POST_DISCOUNT) base -= discount;
  if (base < 0.0) base = 0.0;
  return line(base * rates.provincial_pct / 100.0);
}

/* loyalty ---------------------------------------------------------------- */

double loyalty_discount(double amount, double loyalty_pct) {
  return line(amount * loyalty_pct / 100.0);
}

double apply_loyalty(double amount, double loyalty_pct) {
  return line(amount - loyalty_discount(amount, loyalty_pct));
}

/* invoice orchestration -------------------------------------------------- */

InvoiceInputs::InvoiceInputs() {
  usage_mb = 0;
  included_gb = 0;
  plan_fee = 0.0;
  previous_plan_fee = 0.0;
  plan_change_day = 0;
  line_count = 1;
  promo_amount = 0.0;
  suspension_start_day = 0;
  suspension_end_day = 0;
  prior_balance = 0.0;
  loyalty_pct = 0.0;
}

InvoiceAmounts compute_invoice_amounts(const InvoiceInputs &in) {
  InvoiceAmounts out;
  out.usage_gb_rated = usage_gb_rounded(in.usage_mb);
  out.overage_gb = overage_gb(in.usage_mb, in.included_gb);
  out.overage_mb = overage_mb(in.usage_mb, in.included_gb);

  out.plan_charge = prorated_plan_charge(in.plan_fee, in.previous_plan_fee, in.plan_change_day,
                                         in.period);
  out.line_discount = multi_line_discount(out.plan_charge, in.line_count);
  out.recurring = line(out.plan_charge - out.line_discount);
  out.overage_charges = rate_overage(in.usage_mb, in.included_gb);
  out.suspension_credit = suspension_credit(in.plan_fee, in.suspension_start_day,
                                            in.suspension_end_day, in.period);
  out.promo_credit = promo_credit(in.promo_amount, in.promo_issued_on, in.period);
  out.late_fee = late_fee(in.prior_balance, in.prior_due_date, in.period);

  double subtotal = out.recurring + out.overage_charges + out.late_fee
                    - out.suspension_credit - out.promo_credit;
  if (subtotal < 0.0) subtotal = 0.0;
  out.subtotal = line(subtotal);

  TaxRates rates = rates_for_province(in.province);
  out.loyalty_discount = loyalty_discount(out.subtotal, in.loyalty_pct);
  out.federal_tax = federal_tax(out.subtotal, rates);
  out.provincial_tax = provincial_tax(out.subtotal, out.loyalty_discount, rates);
  out.federal_label = rates.federal_label;
  out.provincial_label = rates.provincial_label;
  out.total = money(out.subtotal - out.loyalty_discount + out.federal_tax + out.provincial_tax);
  return out;
}

}  // namespace billing_rules
