#include "invoice.h"

#include "telco_rules/invoice.h"

/* the register's account and usage records mapped onto the shared invoice
   rules in telco-rules. the Invoice struct and this signature are what
   run_billing, invoice-api and the reconciliation feed depend on. */

Invoice compute_invoice(const Account &a, const UsageRec &u) {
  telco_rules::InvoiceInput in;
  in.period = u.period;
  in.province = a.province;
  in.usage_mb = u.usage_mb;
  in.included_gb = a.included_gb;
  in.plan_fee = a.plan_fee;
  in.prev_plan_fee = a.prev_plan_fee;
  in.plan_change_day = a.plan_chg_day;
  in.line_count = a.line_cnt;
  in.promo_amount = a.promo_amt;
  in.promo_issued_on = a.promo_dt;
  in.suspension_start_day = a.susp_start;
  in.suspension_end_day = a.susp_end;
  in.prior_balance = a.prior_bal;
  in.prior_due_date = a.prior_due;
  in.loyalty_pct = a.loyalty_pct;
  telco_rules::InvoiceLines lines = telco_rules::compute_invoice(in);

  Invoice inv;
  inv.acct_id = a.acct_id;
  inv.billing_ref = a.billing_ref;
  inv.period = u.period;
  inv.province = a.province;
  inv.usage_mb = u.usage_mb;
  inv.usage_gb_rated = lines.usage_gb_rated;
  inv.overage_gb = lines.overage_gb;
  inv.plan_charge = lines.plan_charge;
  inv.line_discount = lines.line_discount;
  inv.recurring = lines.recurring;
  inv.overage_charges = lines.overage_charges;
  inv.suspension_credit_amt = lines.suspension_credit;
  inv.promo_credit_amt = lines.promo_credit;
  inv.late_fee_amt = lines.late_fee;
  inv.subtotal = lines.subtotal;
  inv.loyalty_amt = lines.loyalty_discount;
  inv.federal_tax_amt = lines.federal_tax;
  inv.provincial_tax_amt = lines.provincial_tax;
  inv.federal_label = lines.federal_label;
  inv.provincial_label = lines.provincial_label;
  inv.total = lines.total;
  return inv;
}
