#include <cstdio>
#include <string>
#include <vector>
#include "accounts.h"
#include "retention.h"

/* account pii retention. dry run unless --apply, see billing/RETENTION.

   account-retention close ACCT_ID --on YYYY-MM-DD [--apply]
   account-retention anonymize ACCT_ID --on YYYY-MM-DD [--apply]
   account-retention sweep --as-of YYYY-MM-DD [--apply]
   account-retention report */

static int usage() {
  fprintf(stderr,
          "usage: account-retention close ACCT_ID --on YYYY-MM-DD [--apply]\n"
          "       account-retention anonymize ACCT_ID --on YYYY-MM-DD [--apply]\n"
          "       account-retention sweep --as-of YYYY-MM-DD [--apply]\n"
          "       account-retention report\n"
          "options: --accounts DIR --accounts-csv PATH --locations PATH\n");
  return 2;
}

int main(int argc, char **argv) {
  RetentionStore st;
  st.accounts_dir = "billing/accounts";
  st.accounts_csv = "data/accounts.csv";
  st.locations_csv = "data/locations.csv";
  std::string cmd, acct, on, as_of;
  bool apply = false;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--accounts" && i + 1 < argc) st.accounts_dir = argv[++i];
    else if (a == "--accounts-csv" && i + 1 < argc) st.accounts_csv = argv[++i];
    else if (a == "--locations" && i + 1 < argc) st.locations_csv = argv[++i];
    else if (a == "--on" && i + 1 < argc) on = argv[++i];
    else if (a == "--as-of" && i + 1 < argc) as_of = argv[++i];
    else if (a == "--apply") apply = true;
    else if (cmd.empty()) cmd = a;
    else if (acct.empty()) acct = a;
    else return usage();
  }

  std::vector<Account> accts = load_accounts(st.accounts_dir);
  std::string err;

  if (cmd == "report") {
    printf("%-16s %-8s %-10s %-10s %-10s\n", "ACCT_ID", "STATUS", "CLOSED", "PURGE_ON", "ANONYMIZED");
    for (size_t i = 0; i < accts.size(); i++) {
      const Account &a = accts[i];
      if (!is_closed(a)) continue;
      printf("%-16s %-8s %-10s %-10s %-10s\n", a.acct_id.c_str(), "CLOSED", a.closed_dt.c_str(),
             retention_expiry(a).c_str(), is_anonymized(a) ? a.anonymized_dt.c_str() : "-");
    }
    return 0;
  }

  if (cmd == "close" || cmd == "anonymize") {
    if (acct.empty() || !valid_date(on)) return usage();
    Account a;
    if (!find_account(accts, acct, &a)) {
      fprintf(stderr, "no account record for %s\n", acct.c_str());
      return 1;
    }
    if (!apply) {
      printf("would %s %s on %s (%s), rerun with --apply\n", cmd.c_str(), acct.c_str(), on.c_str(),
             a.rec_path.c_str());
      return 0;
    }
    bool ok = cmd == "close" ? close_account(st, acct, on, &err) : anonymize_account(st, acct, on, &err);
    if (!ok) {
      fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    printf("%s %s on %s\n", cmd == "close" ? "closed" : "anonymized", acct.c_str(), on.c_str());
    return 0;
  }

  if (cmd == "sweep") {
    if (!acct.empty() || !valid_date(as_of)) return usage();
    if (!apply) {
      std::vector<Account> due = expired_accounts(accts, as_of);
      for (size_t i = 0; i < due.size(); i++) {
        printf("would purge %s (closed %s, retention ended %s)\n", due[i].acct_id.c_str(),
               due[i].closed_dt.c_str(), retention_expiry(due[i]).c_str());
      }
      printf("%d account(s) past retention as of %s, rerun with --apply\n", (int)due.size(),
             as_of.c_str());
      return 0;
    }
    std::vector<std::string> purged;
    bool ok = purge_expired(st, as_of, &purged, &err);
    for (size_t i = 0; i < purged.size(); i++) printf("purged %s\n", purged[i].c_str());
    if (!ok) {
      fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    printf("%d account(s) purged as of %s\n", (int)purged.size(), as_of.c_str());
    return 0;
  }

  return usage();
}
