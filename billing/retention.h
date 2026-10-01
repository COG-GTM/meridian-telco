#ifndef MERIDIAN_RETENTION_H
#define MERIDIAN_RETENTION_H

#include <string>
#include <vector>
#include "accounts.h"

/* account pii retention, see billing/RETENTION.

   open account      name, tax id and service address kept, needed to bill.
   closed account    CLOSED_DT starts the clock. the whole record is kept for
                     RETENTION_YEARS so invoices can be reissued and audited.
   erasure request   the closed record is anonymized straight away: CUST_NM,
                     TAX_ID and SVC_ADDR are blanked, the billing figures stay.
   retention expiry  the record is purged: .rec deleted, row dropped from the
                     accounts.csv mirror, customer name taken off locations.csv. */

extern const int RETENTION_YEARS;
extern const char *REDACTED_NAME;

struct RetentionStore {
  std::string accounts_dir;
  std::string accounts_csv;
  std::string locations_csv;
};

bool valid_date(const std::string &s);
bool is_closed(const Account &a);
bool is_anonymized(const Account &a);
std::string retention_expiry(const Account &a);
bool retention_expired(const Account &a, const std::string &as_of);
std::vector<Account> expired_accounts(const std::vector<Account> &accts, const std::string &as_of);

bool close_account(const RetentionStore &st, const std::string &acct_id, const std::string &on,
                   std::string *err);
bool anonymize_account(const RetentionStore &st, const std::string &acct_id, const std::string &on,
                       std::string *err);
bool purge_expired(const RetentionStore &st, const std::string &as_of,
                   std::vector<std::string> *purged, std::string *err);

#endif
