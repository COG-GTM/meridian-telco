#include <cstdio>
#include <cmath>
#include <string>
#include "accounts.h"
#include "invoice.h"
#include "billing_rules/rules.h"

/* billing rule checks. bin/billing-test, run with make check.

   the rules come from the vendored telco-billing-rules binding (billing/vendor).
   these checks pin the profile meridian bills on, so a change to rules.json that
   moves meridian customers shows up here as well as in the vendored conformance
   vectors (bin/rules-conformance). */

using namespace billing_rules;

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
  check_eq("july and february prorate alike",
           prorated_plan_charge(300.0, 600.0, 16, "2026-02"),
           prorated_plan_charge(300.0, 600.0, 16, "2026-07"));

  /* rating: exact megabytes over the allowance at 1.2 cents a mb */
  check_eq("usage under plan rates nothing", rate_overage(10240, 500), 0.0);
  check_eq("one mb over rates one mb", rate_overage(500 * 1024 + 1, 500), 0.01);
  check_eq("1000 mb over rates 1000 mb", rate_overage(2000 * 1024 + 1000, 2000), 12.0);

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

  /* the register goes through the shared invoice formula */
  Account a = Account();
  a.acct_id = "CHECK-0001";
  a.province = "BC";
  a.plan_fee = 640.0;
  a.included_gb = 500;
  a.line_cnt = 3;
  a.loyalty_pct = 10.0;
  UsageRec u;
  u.acct_id = a.acct_id;
  u.period = "2026-07";
  u.usage_mb = 500 * 1024 + 1;
  Invoice inv = compute_invoice(a, u);
  check_eq("recurring is plan less line discount", inv.recurring, 640.0 - 32.0);
  check_eq("subtotal is recurring plus overage", inv.subtotal, inv.recurring + inv.overage_charges);
  check_eq("total is subtotal less loyalty plus tax", inv.total,
           inv.subtotal - inv.loyalty_amt + inv.federal_tax_amt + inv.provincial_tax_amt);

  if (failures) {
    printf("\n%d check(s) failed\n", failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
