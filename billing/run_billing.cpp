#include <cstdio>
#include <cstdlib>
#include <string>
#include "accounts.h"
#include "invoice.h"

/* monthly billing run. prints the invoice register.
   --json prints one record per line for the reconciliation tooling. */

static void print_json(const Invoice &inv, const Account &a) {
  printf("{\"billing_ref\":\"%s\",\"acct_id\":\"%s\",\"period\":\"%s\",\"province\":\"%s\","
         "\"usage_mb\":%ld,\"overage_gb\":%ld,\"plan_charge\":%.2f,\"line_discount\":%.2f,"
         "\"overage_charges\":%.2f,\"suspension_credit\":%.2f,\"promo_credit\":%.2f,"
         "\"late_fee\":%.2f,\"subtotal\":%.2f,\"loyalty\":%.2f,\"federal_tax\":%.2f,"
         "\"provincial_tax\":%.2f,\"total\":%.2f,\"cust_nm\":\"%s\"}\n",
         inv.billing_ref.c_str(), inv.acct_id.c_str(), inv.period.c_str(), inv.province.c_str(),
         inv.usage_mb, inv.overage_gb, inv.plan_charge, inv.line_discount, inv.overage_charges,
         inv.suspension_credit_amt, inv.promo_credit_amt, inv.late_fee_amt, inv.subtotal,
         inv.loyalty_amt, inv.federal_tax_amt, inv.provincial_tax_amt, inv.total,
         a.cust_nm.c_str());
}

int main(int argc, char **argv) {
  std::string accts_dir = "billing/accounts";
  std::string usage_csv = "data/usage.csv";
  std::string period = "";
  int as_json = 0;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--accounts" && i + 1 < argc) accts_dir = argv[++i];
    else if (a == "--usage" && i + 1 < argc) usage_csv = argv[++i];
    else if (a == "--period" && i + 1 < argc) period = argv[++i];
    else if (a == "--json") as_json = 1;
  }
  std::vector<Account> accts = load_accounts(accts_dir);
  std::vector<UsageRec> usage = load_usage(usage_csv);
  double grand = 0.0;
  if (!as_json) {
    printf("%-16s %-8s %-4s %12s %10s %12s %10s %12s\n", "ACCT_ID", "PERIOD", "PRV",
           "USAGE_MB", "OVER_GB", "SUBTOTAL", "TAX", "TOTAL");
  }
  for (size_t i = 0; i < usage.size(); i++) {
    UsageRec u = usage[i];
    if (!period.empty() && u.period != period) continue;
    Account a;
    if (!find_account(accts, u.acct_id, &a)) {
      if (!as_json) {
        printf("%-16s %-8s %-4s %12ld %10s %12s %10s %12s\n", u.acct_id.c_str(),
               u.period.c_str(), "-", u.usage_mb, "-", "-", "-", "NO ACCOUNT");
      }
      continue;
    }
    Invoice inv = compute_invoice(a, u);
    grand += inv.total;
    if (as_json) {
      print_json(inv, a);
      continue;
    }
    printf("%-16s %-8s %-4s %12ld %10ld %12.2f %10.2f %12.2f\n", u.acct_id.c_str(),
           u.period.c_str(), a.province.c_str(), u.usage_mb, inv.overage_gb, inv.subtotal,
           inv.federal_tax_amt + inv.provincial_tax_amt, inv.total);
  }
  if (!as_json) {
    printf("%-16s %-8s %-4s %12s %10s %12s %10s %12.2f\n", "TOTAL", "", "", "", "", "", "", grand);
  }
  return 0;
}
