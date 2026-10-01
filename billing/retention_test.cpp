#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include "accounts.h"
#include "invoice.h"
#include "retention.h"

/* account pii retention checks. bin/retention-test, run with make check. */

static int failures = 0;

static void check_true(const char *what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static void check_str(const char *what, const std::string &got, const std::string &want) {
  if (got != want) {
    printf("FAIL %-52s got '%s' want '%s'\n", what, got.c_str(), want.c_str());
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static void put(const std::string &path, const std::string &body) {
  std::ofstream f(path.c_str());
  f << body;
}

static std::string slurp(const std::string &path) {
  std::ifstream f(path.c_str());
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static bool exists(const std::string &path) {
  return access(path.c_str(), F_OK) == 0;
}

static std::string rec(const std::string &id, const std::string &ref, const std::string &nm,
                       const std::string &tax, const std::string &addr, const std::string &extra) {
  return "ACCT_ID=" + id + "\nBILLING_REF=" + ref + "\nCUST_NM=" + nm + "\nTAX_ID=" + tax +
         "\nSVC_ADDR=" + addr +
         "\nPROVINCE=BC\nPLAN_CD=ENT-2500\nPLAN_FEE=2400.0\nINCLUDED_GB=2500\n"
         "PREV_PLAN_FEE=0.0\nPLAN_CHG_DAY=0\nLINE_CNT=4\nPROMO_AMT=0.0\nPROMO_DT=\n"
         "SUSP_START=0\nSUSP_END=0\nPRIOR_BAL=0.0\nPRIOR_DUE=\nLOYALTY_PCT=2.0\n" + extra;
}

static Account account_with(const std::string &status, const std::string &closed) {
  Account a;
  a.status = status;
  a.closed_dt = closed;
  return a;
}

int main() {
  char tmpl[] = "/tmp/retention-test-XXXXXX";
  char *root = mkdtemp(tmpl);
  if (!root) {
    printf("FAIL cannot create scratch dir\n");
    return 1;
  }
  std::string dir = root;
  RetentionStore st;
  st.accounts_dir = dir + "/accounts";
  st.accounts_csv = dir + "/accounts.csv";
  st.locations_csv = dir + "/locations.csv";
  std::string mk = "mkdir -p " + st.accounts_dir;
  if (system(mk.c_str()) != 0) return 1;

  put(st.accounts_dir + "/T-1.rec",
      rec("T-1", "TN-9001", "Alder Freight Inc", "11-1111111", "1 Main St, Kamloops, BC V2C 1A1", ""));
  put(st.accounts_dir + "/T-2.rec",
      rec("T-2", "TN-9002", "Birch Health", "22-2222222", "2 Main St, Kamloops, BC V2C 1A2",
          "STATUS=CLOSED\nCLOSED_DT=2019-07-31\n"));
  put(st.accounts_dir + "/T-3.rec",
      rec("T-3", "TN-9003", "Cedar Labs", "33-3333333", "3 Main St, Kamloops, BC V2C 1A3",
          "STATUS=CLOSED\nCLOSED_DT=2024-02-29\n"));
  put(st.accounts_dir + "/T-4.rec",
      rec("T-4", "TN-9004", "Alder Freight Inc", "44-4444444", "4 Main St, Kamloops, BC V2C 1A4", ""));
  put(st.accounts_csv,
      "ACCT_ID,BILLING_REF,CUST_NM,TAX_ID,SVC_ADDR,PROVINCE\n"
      "T-1,TN-9001,Alder Freight Inc,11-1111111,\"1 Main St, Kamloops, BC V2C 1A1\",BC\n"
      "T-2,TN-9002,Birch Health,22-2222222,\"2 Main St, Kamloops, BC V2C 1A2\",BC\n"
      "T-3,TN-9003,Cedar Labs,33-3333333,\"3 Main St, Kamloops, BC V2C 1A3\",BC\n"
      "T-4,TN-9004,Alder Freight Inc,44-4444444,\"4 Main St, Kamloops, BC V2C 1A4\",BC\n");
  put(st.locations_csv,
      "LOC_CD,CUST_NM,LOC_NM,MARKET_CD,TOTAL_CAP_MBPS,ALLOC_CAP_MBPS\n"
      "L-1,Alder Freight Inc,Alder Yard,KAM-01,1000,400\n"
      "L-2,Birch Health,Birch Clinic,KAM-01,1000,200\n"
      "L-3,Cedar Labs,Cedar Campus,KAM-02,2000,900\n");

  /* the retention clock */
  check_true("dates are validated", valid_date("2026-07-31") && !valid_date("2026-02-30") &&
                                        !valid_date("2026-7-31") && !valid_date(""));
  check_str("open account has no purge date", retention_expiry(account_with("", "")), "");
  check_str("closed account kept six years", retention_expiry(account_with("CLOSED", "2020-03-15")),
            "2026-03-15");
  check_str("leap day closure purges on feb 28",
            retention_expiry(account_with("CLOSED", "2024-02-29")), "2030-02-28");
  check_true("not expired the day before",
             !retention_expired(account_with("CLOSED", "2020-03-15"), "2026-03-14"));
  check_true("expired on the purge date",
             retention_expired(account_with("CLOSED", "2020-03-15"), "2026-03-15"));
  check_true("open account never expires", !retention_expired(account_with("", ""), "2099-01-01"));

  /* erasure request: anonymize keeps the billing figures */
  std::string err;
  std::vector<Account> before = load_accounts(st.accounts_dir);
  Account t3;
  find_account(before, "T-3", &t3);
  UsageRec u;
  u.acct_id = "T-3";
  u.period = "2026-07";
  u.usage_mb = 3100000;
  double total_before = compute_invoice(t3, u).total;

  check_true("open account cannot be anonymized", !anonymize_account(st, "T-1", "2026-08-01", &err));
  check_true("bad date refused", !anonymize_account(st, "T-3", "2026/08/01", &err));
  check_true("unknown account refused", !anonymize_account(st, "NOPE", "2026-08-01", &err));
  chmod((st.accounts_dir + "/T-3.rec").c_str(), 0600);
  check_true("closed account anonymized", anonymize_account(st, "T-3", "2026-08-01", &err));
  struct stat sb;
  check_true("owner-only mode kept on rewrite",
             stat((st.accounts_dir + "/T-3.rec").c_str(), &sb) == 0 && (sb.st_mode & 07777) == 0600);

  std::vector<Account> after = load_accounts(st.accounts_dir);
  Account a3;
  find_account(after, "T-3", &a3);
  check_str("name redacted on the record", a3.cust_nm, REDACTED_NAME);
  check_str("tax id removed from the record", a3.tax_id, "");
  check_str("service address removed from the record", a3.svc_addr, "");
  check_str("anonymized date recorded", a3.anonymized_dt, "2026-08-01");
  check_str("billing ref kept", a3.billing_ref, "TN-9003");
  check_str("province kept for tax", a3.province, "BC");
  check_true("invoice total unchanged after anonymizing",
             fabs(compute_invoice(a3, u).total - total_before) < 0.005);
  std::string r3 = slurp(st.accounts_dir + "/T-3.rec");
  check_true("no pii left in the .rec file",
             r3.find("Cedar Labs") == std::string::npos && r3.find("33-3333333") == std::string::npos &&
                 r3.find("3 Main St") == std::string::npos);
  std::string csv = slurp(st.accounts_csv);
  check_true("accounts.csv row redacted",
             csv.find("Cedar Labs") == std::string::npos && csv.find("33-3333333") == std::string::npos &&
                 csv.find("T-3,TN-9003,REDACTED,,,BC\n") != std::string::npos);
  check_true("other accounts.csv rows untouched",
             csv.find("T-2,TN-9002,Birch Health,22-2222222,\"2 Main St, Kamloops, BC V2C 1A2\",BC\n") !=
                 std::string::npos);
  std::string loc = slurp(st.locations_csv);
  check_true("locations.csv name redacted",
             loc.find("Cedar Labs") == std::string::npos &&
                 loc.find("L-3,REDACTED,Cedar Campus,KAM-02,2000,900\n") != std::string::npos);
  check_true("no temp files left behind", !exists(st.accounts_dir + "/T-3.rec.tmp") &&
                                              !exists(st.accounts_csv + ".tmp"));

  /* close, then a shared customer name stays on locations while another live account uses it */
  check_true("open account closed", close_account(st, "T-1", "2026-08-01", &err));
  check_true("closing twice refused", !close_account(st, "T-1", "2026-08-02", &err));
  check_true("second account anonymized", anonymize_account(st, "T-1", "2026-08-01", &err));
  loc = slurp(st.locations_csv);
  check_true("shared location name kept for the live account",
             loc.find("L-1,Alder Freight Inc,Alder Yard") != std::string::npos);

  /* retention expiry: the record is purged everywhere */
  std::vector<std::string> purged;
  check_true("nothing purged before retention ends",
             purge_expired(st, "2025-07-30", &purged, &err) && purged.empty());
  check_true("sweep runs", purge_expired(st, "2025-07-31", &purged, &err));
  check_true("only the expired account purged", purged.size() == 1 && purged[0] == "T-2");
  check_true(".rec file deleted", !exists(st.accounts_dir + "/T-2.rec"));
  csv = slurp(st.accounts_csv);
  check_true("accounts.csv row dropped",
             csv.find("T-2,") == std::string::npos && csv.find("Birch Health") == std::string::npos);
  loc = slurp(st.locations_csv);
  check_true("locations.csv name removed on purge", loc.find("Birch Health") == std::string::npos);
  check_true("unexpired records kept", exists(st.accounts_dir + "/T-1.rec") &&
                                           exists(st.accounts_dir + "/T-3.rec") &&
                                           exists(st.accounts_dir + "/T-4.rec"));
  check_true("purged account no longer loads", load_accounts(st.accounts_dir).size() == 3);

  std::string rm = "rm -rf " + dir;
  if (system(rm.c_str()) != 0) printf("warn: could not remove %s\n", dir.c_str());

  if (failures) {
    printf("\n%d check(s) failed\n", failures);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
