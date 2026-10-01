#ifndef MERIDIAN_ACCOUNTS_H
#define MERIDIAN_ACCOUNTS_H

#include <string>
#include <vector>
#include <map>

struct Account {
  std::string acct_id;
  std::string billing_ref;
  std::string cust_nm;
  std::string tax_id;
  std::string svc_addr;
  std::string province;
  std::string plan_cd;
  double plan_fee;
  long included_gb;
  double prev_plan_fee;
  int plan_chg_day;
  int line_cnt;
  double promo_amt;
  std::string promo_dt;
  int susp_start;
  int susp_end;
  double prior_bal;
  std::string prior_due;
  double loyalty_pct;
};

std::vector<Account> load_accounts(const std::string &dir);
bool find_account(const std::vector<Account> &accts, const std::string &id, Account *out);

/* customer identity (CUST_NM, TAX_ID, SVC_ADDR) is not kept in the account master.
   it is read at run time from a csv owned by the running user with mode 0600 or
   tighter. returns the number of accounts matched, -1 with *err set on refusal. */
int load_account_identity(const std::string &path, std::vector<Account> *accts, std::string *err);

/* --identity flag value, else $MERIDIAN_ACCOUNT_IDENTITY, else no identity.
   false (after reporting on stderr) when a configured file is refused. */
bool apply_account_identity(const std::string &flag_value, std::vector<Account> *accts);

struct UsageRec {
  std::string acct_id;
  std::string period;
  long usage_mb;
};

std::vector<UsageRec> load_usage(const std::string &csv_path);

#endif
