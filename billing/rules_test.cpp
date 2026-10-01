#include <cstdio>
#include <cmath>
#include <string>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include "accounts.h"
#include "latefee.h"
#include "lines.h"
#include "promo.h"
#include "proration.h"
#include "suspension.h"
#include "tax.h"

/* billing rule checks. bin/billing-test, run with make check. */

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

static std::string write_identity_file(mode_t mode) {
  char path[] = "/tmp/meridian-identity-XXXXXX";
  int fd = mkstemp(path);
  if (fd < 0) return "";
  const char body[] = "ACCT_ID,CUST_NM,TAX_ID,SVC_ADDR\n"
                      "MER-011000,Example Customer A,00-0000001,\"1 Example St, Edmonton, AB\"\n";
  ssize_t n = write(fd, body, sizeof(body) - 1);
  close(fd);
  if (n != (ssize_t)(sizeof(body) - 1)) return "";
  chmod(path, mode);
  return path;
}

static void identity_checks() {
  std::vector<Account> master = load_accounts("billing/accounts");
  bool clean = !master.empty();
  for (size_t i = 0; i < master.size(); i++) {
    if (!master[i].cust_nm.empty() || !master[i].tax_id.empty() || !master[i].svc_addr.empty())
      clean = false;
  }
  check_true("account master carries no customer identity", clean);

  std::vector<Account> accts(1);
  accts[0].acct_id = "MER-011000";
  std::string err;
  std::string ok_path = write_identity_file(0600);
  check_eq("0600 identity file is loaded", load_account_identity(ok_path, &accts, &err), 1.0);
  check_true("identity fields merged by acct id",
             accts[0].tax_id == "00-0000001" && accts[0].svc_addr == "1 Example St, Edmonton, AB");

  std::vector<Account> fresh(1);
  fresh[0].acct_id = "MER-011000";
  std::string open_path = write_identity_file(0644);
  check_eq("world readable identity file is refused", load_account_identity(open_path, &fresh, &err), -1.0);
  check_true("refused file leaves accounts untouched", fresh[0].tax_id.empty());

  std::string link_path = ok_path + ".lnk";
  check_true("symlink created", symlink(ok_path.c_str(), link_path.c_str()) == 0);
  check_eq("symlinked identity file is refused", load_account_identity(link_path, &fresh, &err), -1.0);
  check_eq("missing identity file is refused", load_account_identity(ok_path + ".missing", &fresh, &err), -1.0);

  unlink(link_path.c_str());
  unlink(ok_path.c_str());
  unlink(open_path.c_str());
}

int main() {
  /* proration is on a 30 day month whatever the calendar says */
  check_eq("full month is the plan fee", prorated_plan_charge(640.0, 0.0, 0), 640.0);
  check_eq("half month on each plan", prorated_plan_charge(300.0, 600.0, 16), 450.0);
  check_eq("july and february prorate alike",
           prorated_plan_charge(300.0, 600.0, 16), prorated_plan_charge(300.0, 600.0, 16));

  /* promo credits belong to the cycle they were issued in */
  check_true("promo issued in cycle is live", promo_is_live("2026-07-04", "2026-07"));
  check_true("promo issued last cycle is dead", !promo_is_live("2026-06-24", "2026-07"));
  check_eq("dead promo credits nothing", promo_credit(120.0, "2026-06-24", "2026-07"), 0.0);

  /* suspended days stay billed */
  check_eq("suspended days counted", (double)suspended_days(10, 14), 5.0);
  check_eq("suspension credits nothing", suspension_credit(620.0, 10, 14), 0.0);

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

  identity_checks();

  if (failures) {
    printf("\n%d check(s) failed\n", failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
