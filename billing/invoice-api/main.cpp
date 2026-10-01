#include <cstdio>
#include <cstdlib>
#include <string>
#include "../../inventory-api/httpd.h"
#include "../accounts.h"
#include "../invoice.h"
#include "../rating.h"
#include "access.h"

/* invoice api. GET /invoices?acct=BEACON-004417[&period=2026-07]
   every request needs "Authorization: Bearer <token>" for a caller in the
   keys file (see access.h). GET /invoices without acct returns the accounts
   the caller is entitled to. every request, allowed or not, is appended to
   the audit log before anything is returned. */

static std::vector<Account> g_accts;
static std::vector<UsageRec> g_usage;
static std::vector<ApiCaller> g_callers;
static std::string g_audit_log;

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

static std::string build_invoices(const HttpRequest &req, const ApiCaller *caller, int *status,
                                  std::vector<std::string> *disclosed) {
  if (req.method != "GET") {
    *status = 405;
    return "{\"error\":\"method not allowed\"}";
  }
  if (!caller) {
    *status = 401;
    return "{\"error\":\"unauthorized\"}";
  }
  std::map<std::string, std::string> q = req.query;
  std::string want_acct = q.count("acct") ? q["acct"] : "";
  std::string want_period = q.count("period") ? q["period"] : "";
  if (!want_acct.empty() && !caller_may_read(*caller, want_acct)) {
    *status = 403;
    return "{\"error\":\"forbidden\"}";
  }
  std::string out = "{\"invoices\":[";
  int n = 0;
  for (size_t i = 0; i < g_usage.size(); i++) {
    UsageRec u = g_usage[i];
    if (!want_acct.empty() && u.acct_id != want_acct) continue;
    if (!want_period.empty() && u.period != want_period) continue;
    if (!caller_may_read(*caller, u.acct_id)) continue;
    Account a;
    if (!find_account(g_accts, u.acct_id, &a)) continue;
    if (n) out += ",";
    out += invoice_json(a, u);
    disclosed->push_back(u.acct_id + "/" + u.period);
    n++;
  }
  out += "]}";
  if (n == 0 && !want_acct.empty()) *status = 404;
  return out;
}

static std::string handle_invoices(const HttpRequest &req, int *status) {
  std::string auth = req.headers.count("authorization") ? req.headers.find("authorization")->second : "";
  const ApiCaller *caller = authenticate(g_callers, auth);
  std::vector<std::string> disclosed;
  std::string body = build_invoices(req, caller, status, &disclosed);

  AuditRecord rec;
  rec.ts = utc_timestamp();
  rec.caller = caller ? caller->id : "-";
  rec.peer = req.peer;
  rec.method = req.method;
  rec.target = req.target;
  rec.status = *status;
  rec.disclosed = disclosed;
  if (!append_audit(g_audit_log, rec)) {
    fprintf(stderr, "audit log %s not writable, withholding response\n", g_audit_log.c_str());
    *status = 503;
    return "{\"error\":\"audit unavailable\"}";
  }
  return body;
}

int main(int argc, char **argv) {
  std::string accts_dir = "billing/accounts";
  std::string usage_csv = "data/usage.csv";
  std::string keys_file = getenv("INVOICE_API_KEYS") ? getenv("INVOICE_API_KEYS") : "";
  std::string audit_log = getenv("INVOICE_API_AUDIT_LOG") ? getenv("INVOICE_API_AUDIT_LOG") : "";
  ServeOptions opts;
  opts.allow_origin = "http://localhost:8083";
  int port = 8082;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--accounts" && i + 1 < argc) accts_dir = argv[++i];
    else if (a == "--usage" && i + 1 < argc) usage_csv = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
    else if (a == "--keys" && i + 1 < argc) keys_file = argv[++i];
    else if (a == "--audit-log" && i + 1 < argc) audit_log = argv[++i];
    else if (a == "--allow-origin" && i + 1 < argc) opts.allow_origin = argv[++i];
  }
  if (keys_file.empty() || audit_log.empty()) {
    fprintf(stderr, "invoice-api needs --keys <file> and --audit-log <file> "
                    "(or INVOICE_API_KEYS / INVOICE_API_AUDIT_LOG)\n");
    return 1;
  }
  if (opts.allow_origin.empty() || opts.allow_origin == "*") {
    fprintf(stderr, "--allow-origin must name a single origin\n");
    return 1;
  }
  std::string err;
  if (!load_api_keys(keys_file, &g_callers, &err)) {
    fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  g_audit_log = audit_log;
  AuditRecord start;
  start.ts = utc_timestamp();
  start.caller = "-";
  start.method = "START";
  start.status = 0;
  if (!append_audit(g_audit_log, start)) {
    fprintf(stderr, "cannot write audit log %s\n", g_audit_log.c_str());
    return 1;
  }
  g_accts = load_accounts(accts_dir);
  g_usage = load_usage(usage_csv);
  if (g_accts.empty()) {
    fprintf(stderr, "no account records under %s\n", accts_dir.c_str());
    return 1;
  }
  fprintf(stderr, "loaded %d accounts, %d usage records, %d api callers\n", (int)g_accts.size(),
          (int)g_usage.size(), (int)g_callers.size());
  std::vector<Route> routes;
  Route r; r.prefix = "/invoices"; r.fn = handle_invoices; routes.push_back(r);
  return serve(port, routes, opts);
}
