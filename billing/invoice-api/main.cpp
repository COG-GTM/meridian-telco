#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include "../../third_party/httplib.h"
#include "../accounts.h"
#include "../invoice.h"
#include "../rating.h"

/* invoice api. GET /invoices?acct=BEACON-004417[&period=2026-07]
   GET /invoices returns every account for the periods we have usage for. */

static std::vector<Account> g_accts;
static std::vector<UsageRec> g_usage;

static std::string json_escape(std::string_view value) {
  std::string out;
  for (char c : value) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}

static std::map<std::string, std::string> query_parameters(std::string_view target) {
  std::map<std::string, std::string> query;
  const auto question = target.find('?');
  if (question == std::string_view::npos) return query;
  auto remaining = target.substr(question + 1);
  while (!remaining.empty()) {
    const auto amp = remaining.find('&');
    const auto item = remaining.substr(0, amp);
    const auto eq = item.find('=');
    if (eq != std::string_view::npos) {
      query[std::string(item.substr(0, eq))] = item.substr(eq + 1);
    }
    if (amp == std::string_view::npos) break;
    remaining.remove_prefix(amp + 1);
  }
  return query;
}

static std::string invoice_json(const Account &a, const UsageRec &u) {
  Invoice inv = compute_invoice(a, u);
  std::string buf(2400, '\0');
  snprintf(buf.data(), buf.size(),
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
  buf.resize(std::char_traits<char>::length(buf.c_str()));
  return buf;
}

static std::string handle_invoices(const httplib::Request &req, int &status) {
  auto q = query_parameters(req.target);
  std::string want_acct = q.count("acct") ? q["acct"] : "";
  std::string want_period = q.count("period") ? q["period"] : "";
  std::string out = "{\"invoices\":[";
  int n = 0;
  for (const auto &u : g_usage) {
    if (!want_acct.empty() && u.acct_id != want_acct) continue;
    if (!want_period.empty() && u.period != want_period) continue;
    const auto account = find_account(g_accts, u.acct_id);
    if (!account) continue;
    if (n) out += ",";
    out += invoice_json(account->get(), u);
    n++;
  }
  out += "]}";
  if (n == 0 && !want_acct.empty()) status = 404;
  return out;
}

int main(int argc, char **argv) {
  std::string accts_dir = "billing/accounts";
  std::string usage_csv = "data/usage.csv";
  int port = 8082;
  for (int i = 1; i < argc; i++) {
    std::string_view a = argv[i];
    if (a == "--accounts" && i + 1 < argc) accts_dir = argv[++i];
    else if (a == "--usage" && i + 1 < argc) usage_csv = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
  }
  g_accts = load_accounts(accts_dir);
  g_usage = load_usage(usage_csv);
  if (g_accts.empty()) {
    fprintf(stderr, "no account records under %s\n", accts_dir.c_str());
    return 1;
  }
  fprintf(stderr, "loaded %d accounts, %d usage records\n", (int)g_accts.size(), (int)g_usage.size());
  httplib::Server server;
  server.set_keep_alive_max_count(1);
  server.set_header_writer([](httplib::Stream &stream, httplib::Headers &headers) -> ssize_t {
    auto *buffer = dynamic_cast<httplib::detail::BufferStream *>(&stream);
    if (!buffer) return -1;
    auto line = buffer->get_buffer();
    // The legacy protocol uses ERROR rather than the standard reason phrase.
    if (line == "HTTP/1.1 404 Not Found\r\n") line = "HTTP/1.1 404 ERROR\r\n";
    *buffer = httplib::detail::BufferStream{};
    buffer->write(line.data(), line.size());
    std::string output;
    for (const auto name : {"Content-Type", "Access-Control-Allow-Origin",
                            "Content-Length", "Connection"}) {
      const auto range = headers.equal_range(name);
      for (auto it = range.first; it != range.second; ++it) {
        output += it->first + ": " + it->second + "\r\n";
      }
      headers.erase(name);
    }
    buffer->write(output.data(), output.size());
    return httplib::detail::write_headers(stream, headers);
  });
  server.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
    res.status = 404;
    std::string body = "{\"error\":\"not found\"}";
    const auto path = std::string_view(req.target).substr(0, req.target.find('?'));
    if (path.starts_with("/invoices")) {
      res.status = 200;
      body = handle_invoices(req, res.status);
    }
    res.set_content(body, "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    return httplib::Server::HandlerResponse::Handled;
  });
  if (!server.bind_to_port("0.0.0.0", port)) {
    fprintf(stderr, "bind failed on port %d\n", port);
    return 1;
  }
  fprintf(stderr, "listening on :%d\n", port);
  return server.listen_after_bind() ? 0 : 1;
}
