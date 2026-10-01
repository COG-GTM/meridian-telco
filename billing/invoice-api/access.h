#ifndef MERIDIAN_INVOICE_ACCESS_H
#define MERIDIAN_INVOICE_ACCESS_H

/* who may read invoices, and the record of what each caller was shown.

   keys file, one caller per line, '#' starts a comment:
     <caller-id> <bearer-token> <scope>
   scope is '*' for every account or a comma separated list of ACCT_IDs.
   tokens are at least 32 characters. the file must not be readable by
   group or other. */

#include <string>
#include <vector>

struct ApiCaller {
  std::string id;
  std::string token;
  bool all_accounts;
  std::vector<std::string> accounts;
};

/* returns false and fills *err when the file is missing, unsafe or has no
   usable callers. */
bool load_api_keys(const std::string &path, std::vector<ApiCaller> *out, std::string *err);
bool parse_api_keys(const std::string &text, std::vector<ApiCaller> *out, std::string *err);

/* matches "Bearer <token>" against the configured callers. */
const ApiCaller *authenticate(const std::vector<ApiCaller> &callers, const std::string &authorization);

bool caller_may_read(const ApiCaller &caller, const std::string &acct_id);

struct AuditRecord {
  std::string ts;
  std::string caller;
  std::string peer;
  std::string method;
  std::string target;
  int status;
  std::vector<std::string> disclosed; /* ACCT_ID/PERIOD pairs returned */
};

std::string audit_line(const AuditRecord &r);
std::string utc_timestamp();

/* appends one line to the audit log. returns false if it could not be
   written in full. */
bool append_audit(const std::string &path, const AuditRecord &r);

#endif
