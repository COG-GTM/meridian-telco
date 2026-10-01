#include "accounts.h"
#include "../mediation/csv.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cstdlib>

/* account master. one flat record per account under billing/accounts/.
   there was a migration to the store planned. the records carry billing terms
   only; customer identity comes from load_account_identity(). */

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
  std::ifstream f(path.c_str());
  std::string line;
  while (std::getline(f, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string k = line.substr(0, eq);
    std::string v = line.substr(eq + 1);
    if (k == "ACCT_ID") a.acct_id = v;
    else if (k == "BILLING_REF") a.billing_ref = v;
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

static std::string account_identity_path(const std::string &flag_value) {
  if (!flag_value.empty()) return flag_value;
  const char *env = getenv("MERIDIAN_ACCOUNT_IDENTITY");
  return env ? env : "";
}

static bool read_protected(const std::string &path, std::string *body, std::string *err) {
  int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) {
    *err = path + ": " + strerror(errno);
    return false;
  }
  struct stat st;
  bool ok = false;
  if (fstat(fd, &st) != 0) *err = path + ": " + strerror(errno);
  else if (!S_ISREG(st.st_mode)) *err = path + ": not a regular file";
  else if (st.st_uid != geteuid()) *err = path + ": not owned by the running user";
  else if (st.st_mode & (S_IRWXG | S_IRWXO)) *err = path + ": group/other access, chmod 600";
  else ok = true;
  if (ok) {
    char buf[8192];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) body->append(buf, n);
    if (n < 0) {
      *err = path + ": " + strerror(errno);
      ok = false;
    }
  }
  close(fd);
  return ok;
}

int load_account_identity(const std::string &path, std::vector<Account> *accts, std::string *err) {
  std::string body;
  if (!read_protected(path, &body, err)) return -1;
  std::istringstream in(body);
  std::string line;
  if (!std::getline(in, line)) {
    *err = path + ": empty";
    return -1;
  }
  std::vector<std::string> hdr = split_line(line);
  std::map<std::string, Row> by_id;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    std::vector<std::string> v = split_line(line);
    Row r;
    for (size_t i = 0; i < hdr.size() && i < v.size(); i++) r[hdr[i]] = v[i];
    if (!r["ACCT_ID"].empty()) by_id[r["ACCT_ID"]] = r;
  }
  int matched = 0;
  for (size_t i = 0; i < accts->size(); i++) {
    Account &a = (*accts)[i];
    std::map<std::string, Row>::iterator it = by_id.find(a.acct_id);
    if (it == by_id.end()) continue;
    a.cust_nm = it->second["CUST_NM"];
    a.tax_id = it->second["TAX_ID"];
    a.svc_addr = it->second["SVC_ADDR"];
    matched++;
  }
  return matched;
}

bool apply_account_identity(const std::string &flag_value, std::vector<Account> *accts) {
  std::string path = account_identity_path(flag_value);
  if (path.empty()) return true;
  std::string err;
  int n = load_account_identity(path, accts, &err);
  if (n < 0) {
    fprintf(stderr, "account identity refused: %s\n", err.c_str());
    return false;
  }
  fprintf(stderr, "account identity loaded for %d accounts\n", n);
  return true;
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
