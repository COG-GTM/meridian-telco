#include "access.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "../../inventory-api/httpd.h"

static const size_t MIN_TOKEN_LEN = 32;

static bool constant_time_eq(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) return false;
  unsigned char d = 0;
  for (size_t i = 0; i < a.size(); i++) d |= (unsigned char)(a[i] ^ b[i]);
  return d == 0;
}

bool parse_api_keys(const std::string &text, std::vector<ApiCaller> *out, std::string *err) {
  std::istringstream in(text);
  std::string line;
  int lineno = 0;
  out->clear();
  while (std::getline(in, line)) {
    lineno++;
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    std::istringstream fields(line);
    std::string id, token, scope, extra;
    if (!(fields >> id)) continue;
    char where[64];
    snprintf(where, sizeof(where), "line %d: ", lineno);
    if (!(fields >> token >> scope) || (fields >> extra)) {
      *err = std::string(where) + "want <caller-id> <token> <scope>";
      return false;
    }
    if (token.size() < MIN_TOKEN_LEN) {
      *err = std::string(where) + "token shorter than 32 characters";
      return false;
    }
    for (size_t i = 0; i < out->size(); i++) {
      if ((*out)[i].id == id || (*out)[i].token == token) {
        *err = std::string(where) + "duplicate caller id or token";
        return false;
      }
    }
    ApiCaller c;
    c.id = id;
    c.token = token;
    c.all_accounts = scope == "*";
    if (!c.all_accounts) {
      std::istringstream accts(scope);
      std::string a;
      while (std::getline(accts, a, ',')) {
        if (!a.empty()) c.accounts.push_back(a);
      }
      if (c.accounts.empty()) {
        *err = std::string(where) + "empty scope";
        return false;
      }
    }
    out->push_back(c);
  }
  if (out->empty()) {
    *err = "no callers configured";
    return false;
  }
  return true;
}

bool load_api_keys(const std::string &path, std::vector<ApiCaller> *out, std::string *err) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    *err = "cannot open " + path;
    return false;
  }
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    close(fd);
    *err = path + " is not a regular file";
    return false;
  }
  if (st.st_mode & (S_IRWXG | S_IRWXO)) {
    close(fd);
    *err = path + " is accessible by group or other, chmod 600 it";
    return false;
  }
  std::string text;
  char buf[4096];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) text.append(buf, (size_t)n);
  close(fd);
  if (n < 0) {
    *err = "cannot read " + path;
    return false;
  }
  if (!parse_api_keys(text, out, err)) {
    *err = path + ": " + *err;
    return false;
  }
  return true;
}

const ApiCaller *authenticate(const std::vector<ApiCaller> &callers, const std::string &authorization) {
  static const char PREFIX[] = "Bearer ";
  const size_t plen = sizeof(PREFIX) - 1;
  if (authorization.size() <= plen || authorization.compare(0, plen, PREFIX) != 0) return 0;
  std::string token = authorization.substr(plen);
  const ApiCaller *found = 0;
  for (size_t i = 0; i < callers.size(); i++) {
    if (constant_time_eq(callers[i].token, token)) found = &callers[i];
  }
  return found;
}

bool caller_may_read(const ApiCaller &caller, const std::string &acct_id) {
  if (caller.all_accounts) return true;
  for (size_t i = 0; i < caller.accounts.size(); i++) {
    if (caller.accounts[i] == acct_id) return true;
  }
  return false;
}

std::string utc_timestamp() {
  time_t now = time(0);
  struct tm t;
  gmtime_r(&now, &t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
  return buf;
}

std::string audit_line(const AuditRecord &r) {
  char status[16];
  snprintf(status, sizeof(status), "%d", r.status);
  std::string o = "{\"ts\":\"" + json_escape(r.ts) + "\",\"caller\":\"" + json_escape(r.caller) +
                  "\",\"peer\":\"" + json_escape(r.peer) + "\",\"method\":\"" + json_escape(r.method) +
                  "\",\"target\":\"" + json_escape(r.target) + "\",\"status\":" + status +
                  ",\"disclosed\":[";
  for (size_t i = 0; i < r.disclosed.size(); i++) {
    if (i) o += ",";
    o += "\"" + json_escape(r.disclosed[i]) + "\"";
  }
  o += "]}\n";
  return o;
}

bool append_audit(const std::string &path, const AuditRecord &r) {
  int fd = open(path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0600);
  if (fd < 0) return false;
  std::string line = audit_line(r);
  ssize_t w = write(fd, line.data(), line.size());
  bool ok = w == (ssize_t)line.size() && fsync(fd) == 0;
  return close(fd) == 0 && ok;
}
