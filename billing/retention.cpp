#include "retention.h"
#include "datecalc.h"
#include "../mediation/csv.h"
#include <cstdio>
#include <fstream>
#include <map>
#include <set>

const int RETENTION_YEARS = 6;
const char *REDACTED_NAME = "REDACTED";

bool valid_date(const std::string &s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
  for (size_t i = 0; i < s.size(); i++) {
    if (i == 4 || i == 7) continue;
    if (s[i] < '0' || s[i] > '9') return false;
  }
  BillDate d = parse_date(s);
  return d.ok && d.m <= 12 && d.d <= calendar_days_in_month(d.y, d.m);
}

bool is_closed(const Account &a) {
  return a.status == "CLOSED" && valid_date(a.closed_dt);
}

bool is_anonymized(const Account &a) {
  return !a.anonymized_dt.empty();
}

std::string retention_expiry(const Account &a) {
  if (!is_closed(a)) return "";
  BillDate d = parse_date(a.closed_dt);
  int y = d.y + RETENTION_YEARS;
  int day = d.d;
  if (day > calendar_days_in_month(y, d.m)) day = calendar_days_in_month(y, d.m);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, d.m, day);
  return buf;
}

bool retention_expired(const Account &a, const std::string &as_of) {
  std::string exp = retention_expiry(a);
  return !exp.empty() && as_of >= exp;
}

std::vector<Account> expired_accounts(const std::vector<Account> &accts, const std::string &as_of) {
  std::vector<Account> out;
  for (size_t i = 0; i < accts.size(); i++) {
    if (retention_expired(accts[i], as_of)) out.push_back(accts[i]);
  }
  return out;
}

static bool read_lines(const std::string &path, std::vector<std::string> *out) {
  std::ifstream f(path.c_str());
  if (!f.good()) return false;
  std::string line;
  while (std::getline(f, line)) out->push_back(line);
  return true;
}

/* write to a sibling temp file and rename over the original so a failed
   write never leaves a half redacted record behind. */
static bool replace_file(const std::string &path, const std::vector<std::string> &lines,
                         std::string *err) {
  std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp.c_str(), std::ios::out | std::ios::trunc);
    for (size_t i = 0; i < lines.size(); i++) f << lines[i] << "\n";
    f.flush();
    if (!f.good()) {
      remove(tmp.c_str());
      *err = "cannot write " + tmp;
      return false;
    }
  }
  if (rename(tmp.c_str(), path.c_str()) != 0) {
    remove(tmp.c_str());
    *err = "cannot replace " + path;
    return false;
  }
  return true;
}

static bool rewrite_rec(const std::string &path, const std::map<std::string, std::string> &set,
                        std::string *err) {
  std::vector<std::string> lines;
  if (!read_lines(path, &lines)) {
    *err = "cannot read " + path;
    return false;
  }
  std::map<std::string, std::string> todo = set;
  for (size_t i = 0; i < lines.size(); i++) {
    size_t eq = lines[i].find('=');
    if (eq == std::string::npos) continue;
    std::map<std::string, std::string>::iterator it = todo.find(lines[i].substr(0, eq));
    if (it == todo.end()) continue;
    lines[i] = it->first + "=" + it->second;
    todo.erase(it);
  }
  for (std::map<std::string, std::string>::iterator it = todo.begin(); it != todo.end(); ++it) {
    lines.push_back(it->first + "=" + it->second);
  }
  return replace_file(path, lines, err);
}

static std::string csv_field(const std::string &v) {
  if (v.find(',') == std::string::npos && v.find('"') == std::string::npos) return v;
  std::string out = "\"";
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] != '"') out += v[i];
  }
  return out + "\"";
}

static std::string csv_join(const std::vector<std::string> &v) {
  std::string out;
  for (size_t i = 0; i < v.size(); i++) {
    if (i) out += ",";
    out += csv_field(v[i]);
  }
  return out;
}

static int column(const std::vector<std::string> &hdr, const std::string &name) {
  for (size_t i = 0; i < hdr.size(); i++) {
    if (hdr[i] == name) return (int)i;
  }
  return -1;
}

/* rows of a csv whose key column equals want are dropped, or have the listed
   columns set to the given values. other lines are copied as they are.
   a missing file is fine, there is nothing to redact in it. */
static bool rewrite_csv(const std::string &path, const std::string &key, const std::string &want,
                        bool drop, const std::map<std::string, std::string> &set,
                        std::string *err) {
  if (path.empty()) return true;
  std::vector<std::string> lines;
  if (!read_lines(path, &lines)) return true;
  if (lines.empty()) return true;
  std::vector<std::string> hdr = split_line(lines[0]);
  int kc = column(hdr, key);
  if (kc < 0) {
    *err = path + " has no " + key + " column";
    return false;
  }
  std::vector<std::string> out;
  out.push_back(lines[0]);
  bool changed = false;
  for (size_t i = 1; i < lines.size(); i++) {
    std::vector<std::string> v = split_line(lines[i]);
    if ((int)v.size() <= kc || v[kc] != want) {
      out.push_back(lines[i]);
      continue;
    }
    changed = true;
    if (drop) continue;
    for (std::map<std::string, std::string>::const_iterator it = set.begin(); it != set.end(); ++it) {
      int c = column(hdr, it->first);
      if (c >= 0 && c < (int)v.size()) v[c] = it->second;
    }
    out.push_back(csv_join(v));
  }
  if (!changed) return true;
  return replace_file(path, out, err);
}

static bool load_one(const RetentionStore &st, const std::string &acct_id,
                     std::vector<Account> *accts, Account *a, std::string *err) {
  *accts = load_accounts(st.accounts_dir);
  if (!find_account(*accts, acct_id, a)) {
    *err = "no account record for " + acct_id;
    return false;
  }
  return true;
}

/* locations.csv is keyed on the customer name. take the name off only when
   no other account that still holds its pii goes by the same name. */
static bool redact_locations(const RetentionStore &st, const std::vector<Account> &accts,
                             const Account &a, const std::set<std::string> &leaving,
                             std::string *err) {
  if (a.cust_nm.empty() || a.cust_nm == REDACTED_NAME) return true;
  for (size_t i = 0; i < accts.size(); i++) {
    const Account &o = accts[i];
    if (o.acct_id == a.acct_id || leaving.count(o.acct_id) || is_anonymized(o)) continue;
    if (o.cust_nm == a.cust_nm) return true;
  }
  std::map<std::string, std::string> set;
  set["CUST_NM"] = REDACTED_NAME;
  return rewrite_csv(st.locations_csv, "CUST_NM", a.cust_nm, false, set, err);
}

bool close_account(const RetentionStore &st, const std::string &acct_id, const std::string &on,
                   std::string *err) {
  if (!valid_date(on)) {
    *err = "close date must be YYYY-MM-DD";
    return false;
  }
  std::vector<Account> accts;
  Account a;
  if (!load_one(st, acct_id, &accts, &a, err)) return false;
  if (is_closed(a)) {
    *err = acct_id + " is already closed on " + a.closed_dt;
    return false;
  }
  std::map<std::string, std::string> set;
  set["STATUS"] = "CLOSED";
  set["CLOSED_DT"] = on;
  return rewrite_rec(a.rec_path, set, err);
}

bool anonymize_account(const RetentionStore &st, const std::string &acct_id, const std::string &on,
                       std::string *err) {
  if (!valid_date(on)) {
    *err = "anonymize date must be YYYY-MM-DD";
    return false;
  }
  std::vector<Account> accts;
  Account a;
  if (!load_one(st, acct_id, &accts, &a, err)) return false;
  if (!is_closed(a)) {
    *err = acct_id + " is still open, close it before anonymizing";
    return false;
  }
  std::map<std::string, std::string> pii;
  pii["CUST_NM"] = REDACTED_NAME;
  pii["TAX_ID"] = "";
  pii["SVC_ADDR"] = "";
  /* derived copies first: if the .rec write fails the name is still there
     to find them on the next run. */
  if (!rewrite_csv(st.accounts_csv, "ACCT_ID", acct_id, false, pii, err)) return false;
  if (!redact_locations(st, accts, a, std::set<std::string>(), err)) return false;
  std::map<std::string, std::string> set = pii;
  set["ANONYMIZED_DT"] = on;
  return rewrite_rec(a.rec_path, set, err);
}

bool purge_expired(const RetentionStore &st, const std::string &as_of,
                   std::vector<std::string> *purged, std::string *err) {
  if (!valid_date(as_of)) {
    *err = "as-of date must be YYYY-MM-DD";
    return false;
  }
  std::vector<Account> accts = load_accounts(st.accounts_dir);
  std::vector<Account> due = expired_accounts(accts, as_of);
  std::map<std::string, std::string> none;
  std::set<std::string> leaving;
  for (size_t i = 0; i < due.size(); i++) leaving.insert(due[i].acct_id);
  for (size_t i = 0; i < due.size(); i++) {
    const Account &a = due[i];
    if (!rewrite_csv(st.accounts_csv, "ACCT_ID", a.acct_id, true, none, err)) return false;
    if (!redact_locations(st, accts, a, leaving, err)) return false;
    if (remove(a.rec_path.c_str()) != 0) {
      *err = "cannot delete " + a.rec_path;
      return false;
    }
    purged->push_back(a.acct_id);
  }
  return true;
}
