#include "accounts.h"
#include "../mediation/csv.h"
#include <dirent.h>
#include <fstream>
#include <cstdlib>

/* account master. one flat record per account under billing/accounts/.
   there was a migration to the store planned. */

static Account parse_rec(const std::string &path) {
  Account a;
  a.plan_fee = 0.0;
  a.included_gb = 0;
  a.prev_plan_fee = 0.0;
  a.plan_chg_day = 0;
  a.line_cnt = 1;
  a.promo_amt = 0.0;
  a.susp_start = 0;
  a.susp_end = 0;
  a.prior_bal = 0.0;
  a.loyalty_pct = 0.0;
  a.rec_path = path;
  std::ifstream f(path.c_str());
  std::string line;
  while (std::getline(f, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    if (k == "ACCT_ID") a.acct_id = v;
    else if (k == "BILLING_REF") a.billing_ref = v;
    else if (k == "CUST_NM") a.cust_nm = v;
    else if (k == "TAX_ID") a.tax_id = v;
    else if (k == "SVC_ADDR") a.svc_addr = v;
    else if (k == "PROVINCE") a.province = v;
    else if (k == "PLAN_CD") a.plan_cd = v;
    else if (k == "PLAN_FEE") a.plan_fee = atof(v.c_str());
    else if (k == "INCLUDED_GB") a.included_gb = atol(v.c_str());
    else if (k == "PREV_PLAN_FEE") a.prev_plan_fee = atof(v.c_str());
    else if (k == "PLAN_CHG_DAY") a.plan_chg_day = atoi(v.c_str());
    else if (k == "LINE_CNT") a.line_cnt = atoi(v.c_str());
    else if (k == "PROMO_AMT") a.promo_amt = atof(v.c_str());
    else if (k == "PROMO_DT") a.promo_dt = v;
    else if (k == "SUSP_START") a.susp_start = atoi(v.c_str());
    else if (k == "SUSP_END") a.susp_end = atoi(v.c_str());
    else if (k == "PRIOR_BAL") a.prior_bal = atof(v.c_str());
    else if (k == "PRIOR_DUE") a.prior_due = v;
    else if (k == "LOYALTY_PCT") a.loyalty_pct = atof(v.c_str());
    else if (k == "STATUS") a.status = v;
    else if (k == "CLOSED_DT") a.closed_dt = v;
    else if (k == "ANONYMIZED_DT") a.anonymized_dt = v;
  }
  return a;
}

std::vector<Account> load_accounts(const std::string &dir) {
  std::vector<Account> out;
  DIR *d = opendir(dir.c_str());
  if (!d) return out;
  struct dirent *e;
  while ((e = readdir(d)) != 0) {
    std::string nm = e->d_name;
    if (nm.size() < 5) continue;
    if (nm.substr(nm.size() - 4) != ".rec") continue;
    out.push_back(parse_rec(dir + "/" + nm));
  }
  closedir(d);
  return out;
}

bool find_account(const std::vector<Account> &accts, const std::string &id, Account *out) {
  for (size_t i = 0; i < accts.size(); i++) {
    if (accts[i].acct_id == id) { *out = accts[i]; return true; }
  }
  return false;
}

std::vector<UsageRec> load_usage(const std::string &csv_path) {
  std::vector<UsageRec> out;
  std::vector<Row> rows = read_csv(csv_path);
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    UsageRec u;
    u.acct_id = r["ACCT_ID"];
    u.period = r["PERIOD"];
    u.usage_mb = atol(r["USAGE_MB"].c_str());
    out.push_back(u);
  }
  return out;
}
