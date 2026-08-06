#include <cstdio>
#include <cstdlib>
#include <string>
#include "httpd.h"
#include "../mediation/store.h"
#include "../mediation/status.h"
#include "../mediation/circuit_counter.h"
#include "../mediation/locations.h"
#include "unified_inventory_rules/capacity.h"

/* inventory api. GET /sites , GET /circuits
   field names are the store's field names, downstream depends on them. */

static Store g_store;
static std::vector<Location> g_locations;

static std::string site_json(Row &r) {
  char buf[1024];
  snprintf(buf, sizeof(buf),
           "{\"ASSET_ID\":%s,\"SITE_NM\":\"%s\",\"SITE_CD\":\"%s\",\"REGION_CD\":\"%s\","
           "\"LAT\":%s,\"LON\":%s,\"STATUS_CD\":%s,\"STATUS_LABEL\":\"%s\","
           "\"TOWER_REG\":\"%s\",\"TOTAL_CAP_MBPS\":%s,\"ALLOC_CAP_MBPS\":%s,"
           "\"AVAIL_CAP_MBPS\":%d}",
           r["ASSET_ID"].c_str(), json_escape(r["SITE_NM"]).c_str(), r["SITE_CD"].c_str(),
           r["REGION_CD"].c_str(), r["LAT"].c_str(), r["LON"].c_str(),
           r["STATUS_CD"].c_str(), status_label(to_int(r["STATUS_CD"])).c_str(),
           r["TOWER_REG"].c_str(), r["TOTAL_CAP_MBPS"].c_str(), r["ALLOC_CAP_MBPS"].c_str(),
           unified_inventory_rules::available_capacity(to_int(r["TOTAL_CAP_MBPS"]),
                                                       to_int(r["ALLOC_CAP_MBPS"])));
  return buf;
}

static std::string handle_sites(const HttpRequest &req, int *status) {
  (void)status;
  std::string sql = "SELECT * FROM SITE";
  std::map<std::string, std::string> q = req.query;
  if (q.count("REGION_CD")) sql += " WHERE REGION_CD='" + q["REGION_CD"] + "'";
  else if (q.count("STATUS_CD")) sql += " WHERE STATUS_CD=" + q["STATUS_CD"];
  sql += " ORDER BY ASSET_ID";
  std::vector<Row> rows = g_store.query(sql);
  std::string out = "{\"count\":";
  char n[32];
  snprintf(n, sizeof(n), "%d", (int)rows.size());
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
  std::map<std::string, std::string> q = req.query;
  std::string sql = "SELECT * FROM CIRCUIT";
  if (q.count("CIRCUIT_ID")) sql += " WHERE CIRCUIT_ID='" + q["CIRCUIT_ID"] + "'";
  sql += " ORDER BY CIRCUIT_ID";
  std::vector<Row> rows = g_store.query(sql);
  std::string out = "{\"count\":";
  char n[32];
  snprintf(n, sizeof(n), "%d", (int)rows.size());
  out += n;
  out += ",\"active_count\":";
  snprintf(n, sizeof(n), "%d", count_active_circuits(rows));
  out += n;
  out += ",\"circuits\":[";
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"CIRCUIT_ID\":\"%s\",\"CIRCUIT_NM\":\"%s\",\"A_ASSET_ID\":%s,"
             "\"Z_ASSET_ID\":%s,\"CAP_MBPS\":%s,\"ALLOC_MBPS\":%s,\"ROLE_CD\":%s,"
             "\"STATUS_CD\":%s,\"AVAIL_MBPS\":%d}",
             r["CIRCUIT_ID"].c_str(), json_escape(r["CIRCUIT_NM"]).c_str(),
             r["A_ASSET_ID"].c_str(), r["Z_ASSET_ID"].c_str(), r["CAP_MBPS"].c_str(),
             r["ALLOC_MBPS"].c_str(), r["ROLE_CD"].c_str(), r["STATUS_CD"].c_str(),
             unified_inventory_rules::available_capacity(to_int(r["CAP_MBPS"]),
                                                         to_int(r["ALLOC_MBPS"])));
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
  std::map<std::string, std::string> q = req.query;
  int requested = q.count("requested") ? atoi(q["requested"].c_str()) : 0;
  std::string market = q.count("market") ? q["market"] : "";
  std::string out = std::string("{\"rule\":\"") + unified_inventory_rules::AVAILABILITY_RULE + "\",";
  char n[32];
  snprintf(n, sizeof(n), "%d", requested);
  out += "\"requested_mbps\":";
  out += n;
  out += ",\"locations\":[";
  int emitted = 0;
  for (size_t i = 0; i < g_locations.size(); i++) {
    Location l = g_locations[i];
    if (!market.empty() && l.market_cd != market) continue;
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"LOC_CD\":\"%s\",\"CUST_NM\":\"%s\",\"LOC_NM\":\"%s\",\"MARKET_CD\":\"%s\","
             "\"TOTAL_CAP_MBPS\":%d,\"ALLOC_CAP_MBPS\":%d,\"MAINT_BUF_MBPS\":%d,"
             "\"AVAIL_CAP_MBPS\":%d,\"UTILIZATION_PCT\":%.2f,\"CAN_SUPPORT\":%s}",
             l.loc_cd.c_str(), json_escape(l.cust_nm).c_str(), json_escape(l.loc_nm).c_str(),
             l.market_cd.c_str(), l.total_cap_mbps, l.alloc_cap_mbps, l.maint_buf_mbps,
             location_available_mbps(l),
             location_utilization_pct(l),
             location_can_support(l, requested) ? "true" : "false");
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
    std::string a = argv[i];
    if (a == "--db" && i + 1 < argc) dbpath = argv[++i];
    else if (a == "--locations" && i + 1 < argc) locations_csv = argv[++i];
    else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
  }
  if (!g_store.open(dbpath)) return 1;
  g_locations = load_locations(locations_csv);
  fprintf(stderr, "loaded %d serviceable locations\n", (int)g_locations.size());
  std::vector<Route> routes;
  Route r1; r1.prefix = "/sites"; r1.fn = handle_sites; routes.push_back(r1);
  Route r2; r2.prefix = "/circuits"; r2.fn = handle_circuits; routes.push_back(r2);
  Route r3; r3.prefix = "/capacity"; r3.fn = handle_capacity; routes.push_back(r3);
  return serve(port, routes);
}
