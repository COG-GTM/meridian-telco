#ifndef MERIDIAN_ACCOUNTS_H
#define MERIDIAN_ACCOUNTS_H

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <string_view>

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
std::optional<std::reference_wrapper<const Account>> find_account(
    const std::vector<Account> &accts, std::string_view id);

struct UsageRec {
  std::string acct_id;
  std::string period;
  long usage_mb;
};

std::vector<UsageRec> load_usage(const std::string &csv_path);

#endif
