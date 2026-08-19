#include <cstdio>
#include <cmath>
#include <string>
#include "billing_rules/rules.h"

/* billing rule checks. bin/billing-test, run with make check.

   the rules themselves now live in the shared library under billing/vendor and
   are covered there by conformance vectors captured from this register. these
   checks stay because they are the meridian rules stated in meridian's own
   words: if a shared-library change ever moves our behaviour, this fails. */

using billing_rules::federal_tax;
using billing_rules::late_fee;
using billing_rules::multi_line_discount;
using billing_rules::multi_line_pct;
using billing_rules::promo_credit;
using billing_rules::promo_is_live;
using billing_rules::prorated_plan_charge;
using billing_rules::provincial_tax;
using billing_rules::rates_for_province;
using billing_rules::rate_overage;
using billing_rules::suspended_days;
using billing_rules::suspension_credit;
using billing_rules::TaxRates;
using billing_rules::usage_gb_rounded;

static int failures = 0;

static void check_eq(const char *what, double got, double want) {
  if (fabs(got - want) > 0.005) {
    printf("FAIL %-52s got %.4f want %.4f\n", what, got, want);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static void check_true(const char *what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

int main() {
  /* proration is on a 30 day month whatever the calendar says */
  check_eq("full month is the plan fee", prorated_plan_charge(640.0, 0.0, 0, "2026-07"), 640.0);
  check_eq("half month on each plan", prorated_plan_charge(300.0, 600.0, 16, "2026-07"), 450.0);
  check_eq("july and february prorate alike", prorated_plan_charge(300.0, 600.0, 16, "2026-07"),
           prorated_plan_charge(300.0, 600.0, 16, "2026-02"));

  /* usage is rated in whole gigabytes, a part gigabyte rounds up */
  check_eq("part gigabyte rounds up", (double)usage_gb_rounded(2049), 3.0);
  check_eq("overage is ten dollars a gigabyte", rate_overage(53248, 50), 20.0);

  /* promo credits belong to the cycle they were issued in */
  check_true("promo issued in cycle is live", promo_is_live("2026-07-04", "2026-07"));
  check_true("promo issued last cycle is dead", !promo_is_live("2026-06-24", "2026-07"));
  check_eq("dead promo credits nothing", promo_credit(120.0, "2026-06-24", "2026-07"), 0.0);

  /* suspended days stay billed */
  check_eq("suspended days counted", (double)suspended_days(10, 14), 5.0);
  check_eq("suspension credits nothing", suspension_credit(620.0, 10, 14, "2026-07"), 0.0);

  /* multi line schedule */
  check_eq("two lines no discount", multi_line_pct(2), 0.0);
  check_eq("three lines five percent", multi_line_pct(3), 5.0);
  check_eq("ten lines ten percent", multi_line_pct(10), 10.0);
  check_eq("discount off recurring", multi_line_discount(1000.0, 12), 100.0);

  /* late fee grace */
  check_eq("inside grace no fee", late_fee(500.0, "2026-06-30", "2026-07"), 0.0);
  check_eq("past grace one and a half percent", late_fee(500.0, "2026-06-01", "2026-07"), 7.5);

  /* canadian tax: GST on the pre discount amount, PST on what is paid */
  TaxRates bc = rates_for_province("BC");
  check_eq("bc gst on pre discount", federal_tax(100.0, bc), 5.0);
  check_eq("bc pst on post discount", provincial_tax(100.0, 10.0, bc), 6.30);
  TaxRates on = rates_for_province("ON");
  check_eq("ontario hst is 13", federal_tax(100.0, on), 13.0);
  check_eq("ontario has no separate provincial line", provincial_tax(100.0, 10.0, on), 0.0);
  TaxRates qc = rates_for_province("QC");
  check_true("quebec provincial line is qst", qc.provincial_label == "QST");
  check_eq("quebec qst on post discount", provincial_tax(1000.0, 100.0, qc), 89.78);
  TaxRates ab = rates_for_province("AB");
  check_eq("alberta has no provincial tax", provincial_tax(100.0, 0.0, ab), 0.0);

  if (failures) {
    printf("\n%d check(s) failed\n", failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
