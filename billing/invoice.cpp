#include "invoice.h"
#include "billing_rules/rules.h"

/* one invoice for one account and one usage record. the register and the
   invoice api both come through here so they cannot drift from each other.
   every rule lives in billing/vendor, the shared library both meridian and
   vantage build from. this file only maps our records onto it. */

Invoice compute_invoice(const Account &a, const UsageRec &u) {
  billing_rules::InvoiceInputs in;
  in.period = u.period;
  in.province = a.province;
  in.usage_mb = u.usage_mb;
  in.included_gb = a.included_gb;
  in.plan_fee = a.plan_fee;
  in.previous_plan_fee = a.prev_plan_fee;
  in.plan_change_day = a.plan_chg_day;
  in.line_count = a.line_cnt;
  in.promo_amount = a.promo_amt;
  in.promo_issued_on = a.promo_dt;
  in.suspension_start_day = a.susp_start;
  in.suspension_end_day = a.susp_end;
  in.prior_balance = a.prior_bal;
  in.prior_due_date = a.prior_due;
  in.loyalty_pct = a.loyalty_pct;

  billing_rules::InvoiceAmounts amt = billing_rules::compute_invoice_amounts(in);

  Invoice inv;
  inv.acct_id = a.acct_id;
  inv.billing_ref = a.billing_ref;
  inv.period = u.period;
  inv.province = a.province;
  inv.usage_mb = u.usage_mb;
  inv.usage_gb_rated = amt.usage_gb_rated;
  inv.overage_gb = amt.overage_gb;
  inv.plan_charge = amt.plan_charge;
  inv.line_discount = amt.line_discount;
  inv.recurring = amt.recurring;
  inv.overage_charges = amt.overage_charges;
  inv.suspension_credit_amt = amt.suspension_credit;
  inv.promo_credit_amt = amt.promo_credit;
  inv.late_fee_amt = amt.late_fee;
  inv.subtotal = amt.subtotal;
  inv.loyalty_amt = amt.loyalty_discount;
  inv.federal_tax_amt = amt.federal_tax;
  inv.provincial_tax_amt = amt.provincial_tax;
  inv.federal_label = amt.federal_label;
  inv.provincial_label = amt.provincial_label;
  inv.total = amt.total;
  return inv;
}
