#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include "../../inventory-api/httpd.h"
#include "../accounts.h"
#include "../invoice.h"
#include "../rating.h"

/* invoice api. GET /invoices?acct=BEACON-004417[&period=2026-07]
   GET /invoices returns every account the caller is entitled to for the
   periods we have usage for.

   every request needs "Authorization: Bearer <token>". tokens live in the
   clients file (--clients, see clients.example), one caller per line:
     <caller-id> <token> <* | ACCT_ID[,ACCT_ID...]>
   every request, allowed or denied, is appended to the audit log
   (--audit-log) with the caller and the accounts disclosed. */

struct ApiClient {
  std::string caller;
  std::string token;
  bool all_accts;
  std::set<std::string> accts;
};

static std::vector<Account> g_accts;
static std::vector<UsageRec> g_usage;
static std::vector<ApiClient> g_clients;
static FILE *g_audit = 0;

static bool load_clients(const std::string &path, std::vector<ApiClient> *out) {
  std::ifstream in(path.c_str());
  if (!in) {
    fprintf(stderr, "cannot read clients file %s\n", path.c_str());
    return false;
  }
  struct stat st;
  if (stat(path.c_str(), &st) == 0 && (st.st_mode & 077))
    fprintf(stderr, "warning: clients file %s is readable by group/other, chmod 600 it\n", path.c_str());
  std::string line;
  int ln = 0;
  while (std::getline(in, line)) {
    ln++;
    size_t h = line.find('#');
    if (h != std::string::npos) line = line.substr(0, h);
    std::istringstream ss(line);
    ApiClient c;
    std::string scope;
    if (!(ss >> c.caller)) continue;
    if (!(ss >> c.token >> scope)) {
      fprintf(stderr, "%s:%d: expected <caller-id> <token> <scope>\n", path.c_str(), ln);
      return false;
    }
    if (c.token.size() < 32) {
      fprintf(stderr, "%s:%d: token for %s is shorter than 32 characters\n", path.c_str(), ln, c.caller.c_str());
      return false;
    }
    c.all_accts = scope == "*";
    if (!c.all_accts) {
      std::istringstream as(scope);
      std::string acct;
      while (std::getline(as, acct, ',')) if (!acct.empty()) c.accts.insert(acct);
    }
    out->push_back(c);
  }
  return true;
}

static bool token_equal(const std::string &a, const std::string &b) {
  unsigned char diff = a.size() != b.size();
  size_t n = a.size() < b.size() ? a.size() : b.size();
  for (size_t i = 0; i < n; i++) diff |= (unsigned char)(a[i] ^ b[i]);
  return diff == 0;
}

static const ApiClient *authenticate(const HttpRequest &req) {
  std::map<std::string, std::string>::const_iterator h = req.headers.find("authorization");
  if (h == req.headers.end()) return 0;
  const std::string prefix = "Bearer ";
  if (h->second.compare(0, prefix.size(), prefix) != 0) return 0;
  std::string tok = h->second.substr(prefix.size());
  const ApiClient *match = 0;
  for (size_t i = 0; i < g_clients.size(); i++)
    if (token_equal(tok, g_clients[i].token)) match = &g_clients[i];
  return match;
}

static bool entitled(const ApiClient &c, const std::string &acct) {
  return c.all_accts || c.accts.count(acct) > 0;
}

static std::string audit_field(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char ch = (unsigned char)s[i];
    o += (ch < 0x20 || ch == 0x7f || ch == ' ' || ch == '"') ? '_' : (char)ch;
  }
  return o.empty() ? "-" : o;
}

static void audit(const HttpRequest &req, const ApiClient *c, const char *decision, int status,
                  const std::string &want_acct, const std::string &want_period,
                  const std::vector<std::string> &disclosed) {
  char ts[32];
  time_t now = time(0);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tmv);
  std::string list;
  for (size_t i = 0; i < disclosed.size(); i++) {
    if (i) list += ",";
    list += disclosed[i];
  }
  fprintf(g_audit,
          "%s peer=%s caller=%s decision=%s status=%d method=%s path=%s acct=%s period=%s "
          "count=%d accts=%s\n",
          ts, audit_field(req.peer).c_str(), c ? audit_field(c->caller).c_str() : "-", decision,
          status, audit_field(req.method).c_str(), audit_field(req.path).c_str(),
          audit_field(want_acct).c_str(), audit_field(want_period).c_str(), (int)disclosed.size(),
          list.empty() ? "-" : list.c_str());
  fflush(g_audit);
  fsync(fileno(g_audit));
}

static std::string invoice_json(const Account &a, const UsageRec &u) {
  Invoice inv = compute_invoice(a, u);
  char buf[2400];
  snprintf(buf, sizeof(buf),
           "{\"ACCT_ID\":\"%s\",\"BILLING_REF\":\"%s\",\"CUST_NM\":\"%s\",\"TAX_ID\":\"%s\","
           "\"SVC_ADDR\":\"%s\",\"PROVINCE\":\"%s\",\"PERIOD\":\"%s\",\"PLAN_CD\":\"%s\","
           "\"USAGE_MB\":%ld,\"USAGE_GB_RATED\":%ld,\"INCLUDED_GB\":%ld,\"OVERAGE_GB\":%ld,"
           "\"PLAN_CHARGE\":%.2f,\"LINE_DISCOUNT\":%.2f,\"RECURRING\":%.2f,"
           "\"OVERAGE_CHARGES\":%.2f,\"SUSPENSION_CREDIT\":%.2f,\"PROMO_CREDIT\":%.2f,"
           "\"LATE_FEE\":%.2f,\"SUBTOTAL\":%.2f,\"LOYALTY_PCT\":%.2f,\"LOYALTY\":%.2f,"
           "\"FED_TAX_LBL\":\"%s\",\"FED_TAX\":%.2f,\"PROV_TAX_LBL\":\"%s\",\"PROV_TAX\":%.2f,"
           "\"INVOICE_TOTAL\":%.2f}",
           a.acct_id.c_str(), a.billing_ref.c_str(), json_escape(a.cust_nm).c_str(),
           a.tax_id.c_str(), json_escape(a.svc_addr).c_str(), a.province.c_str(),
           u.period.c_str(), a.plan_cd.c_str(), inv.usage_mb, inv.usage_gb_rated,
           a.included_gb, inv.overage_gb, inv.plan_charge, inv.line_discount, inv.recurring,
           inv.overage_charges, inv.suspension_credit_amt, inv.promo_credit_amt,
           inv.late_fee_amt, inv.subtotal, a.loyalty_pct, inv.loyalty_amt,
           inv.federal_label.c_str(), inv.federal_tax_amt, inv.provincial_label.c_str(),
           inv.provincial_tax_amt, inv.total);
  return buf;
}

static std::string handle_invoices(const HttpRequest &req, int *status) {
  std::map<std::string, std::string> q = req.query;
  std::string want_acct = q.count("acct") ? q["acct"] : "";
  std::string want_period = q.count("period") ? q["period"] : "";
  std::vector<std::string> disclosed;
  const ApiClient *caller = authenticate(req);
  if (!caller) {
    *status = 401;
    audit(req, 0, "deny", *status, want_acct, want_period, disclosed);
    return "{\"error\":\"unauthorized\"}";
  }
  if (req.method != "GET") {
    *status = 405;
    audit(req, caller, "deny", *status, want_acct, want_period, disclosed);
    return "{\"error\":\"method not allowed\"}";
  }
  if (!want_acct.empty() && !entitled(*caller, want_acct)) {
    *status = 403;
    audit(req, caller, "deny", *status, want_acct, want_period, disclosed);
    return "{\"error\":\"forbidden\"}";
  }
  std::string out = "{\"invoices\":[";
  int n = 0;
  for (size_t i = 0; i < g_usage.size(); i++) {
    UsageRec u = g_usage[i];
    if (!want_acct.empty() && u.acct_id != want_acct) continue;
    if (!want_period.empty() && u.period != want_period) continue;
    if (!entitled(*caller, u.acct_id)) continue;
    Account a;
    if (!find_account(g_accts, u.acct_id, &a)) continue;
    if (n) out += ",";
    out += invoice_json(a, u);
    disclosed.push_back(u.acct_id + "/" + u.period);
    n++;
  }
  out += "]}";
  if (n == 0 && !want_acct.empty()) *status = 404;
  audit(req, caller, "allow", *status, want_acct, want_period, disclosed);
  return out;
}

int main(int argc, char **argv) {
  std::string accts_dir = "billing/accounts";
  std::string usage_csv = "data/usage.csv";
  std::string clients_path = "billing/invoice-api/clients.conf";
  std::string audit_path = "invoice-api-audit.log";
  int port = 8082;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--accounts" && i + 1 < argc) accts_dir = argv[++i];
    else if (a == "--usage" && i + 1 < argc) usage_csv = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
    else if (a == "--clients" && i + 1 < argc) clients_path = argv[++i];
    else if (a == "--audit-log" && i + 1 < argc) audit_path = argv[++i];
  }
  if (!load_clients(clients_path, &g_clients)) return 1;
  if (g_clients.empty()) {
    fprintf(stderr, "no api clients in %s, refusing to serve invoices unauthenticated\n",
            clients_path.c_str());
    return 1;
  }
  g_audit = fopen(audit_path.c_str(), "a");
  if (!g_audit) {
    fprintf(stderr, "cannot open audit log %s\n", audit_path.c_str());
    return 1;
  }
  g_accts = load_accounts(accts_dir);
  g_usage = load_usage(usage_csv);
  if (g_accts.empty()) {
    fprintf(stderr, "no account records under %s\n", accts_dir.c_str());
    return 1;
  }
  fprintf(stderr, "loaded %d accounts, %d usage records, %d api clients, audit log %s\n",
          (int)g_accts.size(), (int)g_usage.size(), (int)g_clients.size(), audit_path.c_str());
  std::vector<Route> routes;
  Route r; r.prefix = "/invoices"; r.fn = handle_invoices; routes.push_back(r);
  return serve(port, routes);
}
