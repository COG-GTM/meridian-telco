#include "invoice.h"
#include "rules_profile.h"

/* one invoice for one account and one usage record. the register and the
   invoice api both come through here so they cannot drift from each other. */

Invoice compute_invoice(const Account &a, const UsageRec &u) {
  telco_rules::Account shared;
  shared.province = a.province;
  shared.plan_fee = a.plan_fee;
  shared.included_gb = a.included_gb;
  shared.prev_plan_fee = a.prev_plan_fee;
  shared.plan_chg_day = a.plan_chg_day;
  shared.line_cnt = a.line_cnt;
  shared.promo_amt = a.promo_amt;
  shared.promo_dt = a.promo_dt;
  shared.susp_start = a.susp_start;
  shared.susp_end = a.susp_end;
  shared.prior_bal = a.prior_bal;
  shared.prior_due = a.prior_due;
  shared.loyalty_pct = a.loyalty_pct;
  telco_rules::Invoice shared_invoice =
      telco_rules::compute_invoice(billing_profile(), shared, u.usage_mb, u.period);

  Invoice inv;
  inv.acct_id = a.acct_id;
  inv.billing_ref = a.billing_ref;
  inv.period = u.period;
  inv.province = a.province;
  inv.usage_mb = shared_invoice.usage_mb;
  inv.usage_gb_rated = shared_invoice.usage_gb_rated;
  inv.overage_gb = shared_invoice.overage_gb;
  inv.plan_charge = shared_invoice.plan_charge;
  inv.line_discount = shared_invoice.line_discount;
  inv.recurring = shared_invoice.recurring;
  inv.overage_charges = shared_invoice.overage_charges;
  inv.suspension_credit_amt = shared_invoice.suspension_credit;
  inv.promo_credit_amt = shared_invoice.promo_credit;
  inv.late_fee_amt = shared_invoice.late_fee;
  inv.subtotal = shared_invoice.subtotal;
  inv.loyalty_amt = shared_invoice.loyalty;
  inv.federal_tax_amt = shared_invoice.federal_tax;
  inv.provincial_tax_amt = shared_invoice.provincial_tax;
  inv.federal_label = shared_invoice.federal_label;
  inv.provincial_label = shared_invoice.provincial_label;
  inv.total = shared_invoice.total;
  return inv;
}
