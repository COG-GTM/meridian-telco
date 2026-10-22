#ifndef BILLING_RULES_RULES_H
#define BILLING_RULES_RULES_H

#include <string>

#include "billing_rules/config.h"

/* Shared Canadian billing rules. One implementation of every rule, mirrored by
   the Python binding in python/billing_rules. Behaviour is driven by the
   profile in rules.json; the default profile is the meridian one. */

namespace billing_rules {

/* money ------------------------------------------------------------------ */

/* Half-up to cents. Under the per_line profile every charge line is rounded as
   it is produced and the total is the sum of the rounded lines. */
double money(double amount);
bool rounds_per_line();

/* proration -------------------------------------------------------------- */

int billing_days_in_period(const std::string &period);
double daily_rate(double monthly_fee, const std::string &period);
double prorated_plan_charge(double monthly_fee, double previous_monthly_fee, int change_day,
                            const std::string &period);

/* rating ----------------------------------------------------------------- */

long usage_gb_rounded(long usage_mb);
long overage_gb(long usage_mb, long included_gb);
long overage_mb(long usage_mb, long included_gb);
double rate_overage(long usage_mb, long included_gb);

/* promo ------------------------------------------------------------------ */

bool promo_is_live(const std::string &issued_on, const std::string &period);
double promo_credit(double amount, const std::string &issued_on, const std::string &period);

/* suspension ------------------------------------------------------------- */

int suspended_days(int start_day, int end_day);
double suspension_credit(double monthly_fee, int start_day, int end_day, const std::string &period);

/* multi line ------------------------------------------------------------- */

double multi_line_pct(int line_count);
double multi_line_discount(double recurring_charge, int line_count);

/* late fee --------------------------------------------------------------- */

long days_past_due(const std::string &due_date, const std::string &period);
double late_fee(double prior_balance, const std::string &due_date, const std::string &period);

/* tax -------------------------------------------------------------------- */

struct TaxRates {
  double federal_pct;    /* GST, or HST where the province is harmonized */
  double provincial_pct; /* PST or QST, zero in a harmonized province */
  std::string federal_label;
  std::string provincial_label;
};

TaxRates rates_for_province(const std::string &province);
double federal_tax(double pre_discount_amount, const TaxRates &rates);
double provincial_tax(double pre_discount_amount, double discount, const TaxRates &rates);

/* loyalty ---------------------------------------------------------------- */

double loyalty_discount(double amount, double loyalty_pct);
double apply_loyalty(double amount, double loyalty_pct);

/* invoice orchestration -------------------------------------------------- */

struct InvoiceInputs {
  std::string period;
  std::string province;
  long usage_mb;
  long included_gb;
  double plan_fee;
  double previous_plan_fee;
  int plan_change_day;
  int line_count;
  double promo_amount;
  std::string promo_issued_on;
  int suspension_start_day;
  int suspension_end_day;
  double prior_balance;
  std::string prior_due_date;
  double loyalty_pct;

  InvoiceInputs();
};

struct InvoiceAmounts {
  long usage_gb_rated;
  long overage_gb;
  long overage_mb;
  double plan_charge;
  double line_discount;
  double recurring;
  double overage_charges;
  double suspension_credit;
  double promo_credit;
  double late_fee;
  double subtotal;
  double loyalty_discount;
  double federal_tax;
  double provincial_tax;
  std::string federal_label;
  std::string provincial_label;
  double total;
};

/* The one invoice formula:
     recurring = plan_charge - line_discount
     subtotal  = max(recurring + overage + late_fee - suspension - promo, 0)
     total     = subtotal - loyalty + federal_tax + provincial_tax          */
InvoiceAmounts compute_invoice_amounts(const InvoiceInputs &in);

}  // namespace billing_rules

#endif
