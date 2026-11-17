#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <map>
#include "../third_party/httplib.h"
#include "../mediation/store.h"
#include "../mediation/capacity.h"
#include "../mediation/status.h"
#include "../mediation/circuit_counter.h"
#include "../mediation/locations.h"

/* inventory api. GET /sites , GET /circuits
   field names are the store's field names, downstream depends on them. */

using HttpRequest = std::map<std::string, std::string>;
static std::string json_escape(std::string_view s) {
  std::string out;
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}
static HttpRequest parse_query(std::string_view target) {
  HttpRequest q;
  auto pos = target.find('?');
  if (pos == std::string_view::npos) return q;
  target.remove_prefix(pos + 1);
  while (!target.empty()) {
    auto amp = target.find('&');
    auto kv = target.substr(0, amp == std::string_view::npos ? target.size() : amp);
    auto eq = kv.find('=');
    if (eq != std::string_view::npos)
      q[std::string(kv.substr(0, eq))] = std::string(kv.substr(eq + 1));
    if (amp == std::string_view::npos) break;
    target.remove_prefix(amp + 1);
  }
  return q;
}
static Store g_store;
static std::vector<Location> g_locations;

static std::string site_json(Row &r) {
  std::string buf;
  buf = (std::string("{\"ASSET_ID\":") + r["ASSET_ID"] +
      std::string(",\"SITE_NM\":\"") + json_escape(r["SITE_NM"]) +
      std::string("\",\"SITE_CD\":\"") + r["SITE_CD"] +
      std::string("\",\"REGION_CD\":\"") + r["REGION_CD"] +
      std::string("\",\"LAT\":") + r["LAT"] +
      std::string(",\"LON\":") + r["LON"] +
      std::string(",\"STATUS_CD\":") + r["STATUS_CD"] +
      std::string(",\"STATUS_LABEL\":\"") + status_label(to_int(r["STATUS_CD"])) +
      std::string("\",\"TOWER_REG\":\"") + r["TOWER_REG"] +
      std::string("\",\"TOTAL_CAP_MBPS\":") + r["TOTAL_CAP_MBPS"] +
      std::string(",\"ALLOC_CAP_MBPS\":") + r["ALLOC_CAP_MBPS"] +
      std::string(",\"AVAIL_CAP_MBPS\":") + std::to_string(available_capacity(to_int(r["TOTAL_CAP_MBPS"]), to_int(r["ALLOC_CAP_MBPS"]))) +
      std::string("}")).substr(0, 1023);
  return buf;
}

static std::string handle_sites(const HttpRequest &req, int *status) {
  (void)status;
  std::string sql = "SELECT * FROM SITE";
  auto q = req;
  if (q.count("REGION_CD")) sql += " WHERE REGION_CD='" + q["REGION_CD"] + "'";
  else if (q.count("STATUS_CD")) sql += " WHERE STATUS_CD=" + q["STATUS_CD"];
  sql += " ORDER BY ASSET_ID";
  std::vector<Row> rows = g_store.query(sql);
  std::string out = "{\"count\":";
  std::string n;
  n = (std::to_string(static_cast<int>(rows.size()))).substr(0, 31);
  out += n;
  out += ",\"sites\":[";
  for (size_t i = 0; i < rows.size(); i++) {
    if (i) out += ",";
    out += site_json(rows[i]);
  }
  out += "]}";
  return out;
}

static std::string handle_circuits(const HttpRequest &req, int *status) {
  (void)status;
  auto q = req;
  std::string sql = "SELECT * FROM CIRCUIT";
  if (q.count("CIRCUIT_ID")) sql += " WHERE CIRCUIT_ID='" + q["CIRCUIT_ID"] + "'";
  sql += " ORDER BY CIRCUIT_ID";
  std::vector<Row> rows = g_store.query(sql);
  std::string out = "{\"count\":";
  std::string n;
  n = (std::to_string(static_cast<int>(rows.size()))).substr(0, 31);
  out += n;
  out += ",\"active_count\":";
  n = (std::to_string(count_active_circuits(rows))).substr(0, 31);
  out += n;
  out += ",\"circuits\":[";
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    std::string buf;
    buf = (std::string("{\"CIRCUIT_ID\":\"") + r["CIRCUIT_ID"] +
      std::string("\",\"CIRCUIT_NM\":\"") + json_escape(r["CIRCUIT_NM"]) +
      std::string("\",\"A_ASSET_ID\":") + r["A_ASSET_ID"] +
      std::string(",\"Z_ASSET_ID\":") + r["Z_ASSET_ID"] +
      std::string(",\"CAP_MBPS\":") + r["CAP_MBPS"] +
      std::string(",\"ALLOC_MBPS\":") + r["ALLOC_MBPS"] +
      std::string(",\"ROLE_CD\":") + r["ROLE_CD"] +
      std::string(",\"STATUS_CD\":") + r["STATUS_CD"] +
      std::string(",\"AVAIL_MBPS\":") + std::to_string(available_capacity(to_int(r["CAP_MBPS"]), to_int(r["ALLOC_MBPS"]))) +
      std::string("}")).substr(0, 1023);
    if (i) out += ",";
    out += buf;
  }
  out += "]}";
  return out;
}

/* sales capacity check. one row per serviceable customer location.
   requested= is the bandwidth the rep is quoting, in mbps. */
static std::string handle_capacity(const HttpRequest &req, int *status) {
  (void)status;
  auto q = req;
  int requested = q.count("requested") ? atoi(q["requested"].c_str()) : 0;
  std::string market = q.count("market") ? q["market"] : "";
  std::string out = "{\"rule\":\"AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS\",";
  std::string n;
  n = (std::to_string(requested)).substr(0, 31);
  out += "\"requested_mbps\":";
  out += n;
  out += ",\"locations\":[";
  int emitted = 0;
  for (size_t i = 0; i < g_locations.size(); i++) {
    Location l = g_locations[i];
    if (!market.empty() && l.market_cd != market) continue;
    std::string buf;
    buf = (std::string("{\"LOC_CD\":\"") + l.loc_cd +
      std::string("\",\"CUST_NM\":\"") + json_escape(l.cust_nm) +
      std::string("\",\"LOC_NM\":\"") + json_escape(l.loc_nm) +
      std::string("\",\"MARKET_CD\":\"") + l.market_cd +
      std::string("\",\"TOTAL_CAP_MBPS\":") + std::to_string(l.total_cap_mbps) +
      std::string(",\"ALLOC_CAP_MBPS\":") + std::to_string(l.alloc_cap_mbps) +
      std::string(",\"AVAIL_CAP_MBPS\":") + std::to_string(location_available_mbps(l)) +
      std::string(",\"UTILIZATION_PCT\":") + std::to_string(utilization_pct(l.total_cap_mbps, l.alloc_cap_mbps)) +
      std::string(",\"CAN_SUPPORT\":") + (location_can_support(l, requested) ? "true" : "false") +
      std::string("}")).substr(0, 1023);
    if (emitted) out += ",";
    out += buf;
    emitted++;
  }
  out += "]}";
  return out;
}

int main(int argc, char **argv) {
  std::string dbpath = "meridian.db";
  std::string locations_csv = "data/locations.csv";
  int port = 8081;
  for (int i = 1; i < argc; i++) {
    std::string_view a = argv[i];
    if (a == "--db" && i + 1 < argc) dbpath = argv[++i];
    else if (a == "--locations" && i + 1 < argc) locations_csv = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
  }
  if (!g_store.open(dbpath)) return 1;
  g_locations = load_locations(locations_csv);
  fprintf(stderr, "loaded %d serviceable locations\n", static_cast<int>(g_locations.size()));
  httplib::Server server;
  server.set_keep_alive_max_count(1);
  server.set_header_writer([](httplib::Stream &stream, httplib::Headers &headers) -> ssize_t {
    auto *buffer = dynamic_cast<httplib::detail::BufferStream *>(&stream);
    if (!buffer) return -1;
    auto line = buffer->get_buffer();
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
  server.new_task_queue = [] { return new httplib::ThreadPool(1); };
  server.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
    const std::string_view target = req.target;
    const auto path = target.substr(0, target.find('?'));
    const auto query = parse_query(target);
    int status = 200;
    std::string body;
    if (path.starts_with("/sites")) body = handle_sites(query, &status);
    else if (path.starts_with("/circuits")) body = handle_circuits(query, &status);
    else if (path.starts_with("/capacity")) body = handle_capacity(query, &status);
    else {
      status = 404;
      body = "{\"error\":\"not found\"}";
      res.reason = "ERROR";
    }
    res.status = status;
    res.set_content(body, "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    return httplib::Server::HandlerResponse::Handled;
  });
  if (!server.bind_to_port("0.0.0.0", static_cast<unsigned short>(port))) {
    fprintf(stderr, "bind failed on port %d\n", port);
    return 1;
  }
  fprintf(stderr, "listening on :%d\n", port);
  return server.listen_after_bind() ? 0 : 1;
}
