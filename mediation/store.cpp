#include "store.h"
#include <cstdio>
#include <cstdlib>

Store::Store() : db(0) {}

Store::~Store() { close(); }

bool Store::open(const std::string &path) {
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    fprintf(stderr, "cannot open store %s\n", path.c_str());
    return false;
  }
  sqlite3_exec(db, "PRAGMA synchronous=OFF", 0, 0, 0);
  return true;
}

void Store::close() {
  if (db) { sqlite3_close(db); db = 0; }
}

void Store::create_schema() {
  const char *ddl =
      "DROP TABLE IF EXISTS SITE;"
      "DROP TABLE IF EXISTS CIRCUIT;"
      "CREATE TABLE SITE("
      " ASSET_ID INTEGER PRIMARY KEY,"
      " SITE_NM TEXT, SITE_CD TEXT, REGION_CD TEXT,"
      " LAT REAL, LON REAL, STATUS_CD INTEGER, TOWER_REG TEXT,"
      " TOTAL_CAP_MBPS INTEGER, ALLOC_CAP_MBPS INTEGER);"
      "CREATE TABLE CIRCUIT("
      " CIRCUIT_ID TEXT PRIMARY KEY, CIRCUIT_NM TEXT,"
      " A_ASSET_ID INTEGER, Z_ASSET_ID INTEGER,"
      " CAP_MBPS INTEGER, ALLOC_MBPS INTEGER,"
      " ROLE_CD INTEGER, STATUS_CD INTEGER);";
  char *err = 0;
  if (sqlite3_exec(db, ddl, 0, 0, &err) != SQLITE_OK) {
    fprintf(stderr, "ddl failed: %s\n", err ? err : "?");
    if (err) sqlite3_free(err);
  }
}

static std::string esc(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\'') o += "''";
    else o += s[i];
  }
  return o;
}

void Store::load_sites(const std::vector<Row> &rows) {
  sqlite3_exec(db, "BEGIN", 0, 0, 0);
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO SITE VALUES(%d,'%s','%s','%s',%f,%f,%d,'%s',%d,%d)",
             to_int(r["ASSET_ID"]), esc(r["SITE_NM"]).c_str(), esc(r["SITE_CD"]).c_str(),
             esc(r["REGION_CD"]).c_str(), to_dbl(r["LAT"]), to_dbl(r["LON"]),
             to_int(r["STATUS_CD"]), esc(r["TOWER_REG"]).c_str(),
             to_int(r["TOTAL_CAP_MBPS"]), to_int(r["ALLOC_CAP_MBPS"]));
    sqlite3_exec(db, sql, 0, 0, 0);
  }
  sqlite3_exec(db, "COMMIT", 0, 0, 0);
}

void Store::load_circuits(const std::vector<Row> &rows) {
  sqlite3_exec(db, "BEGIN", 0, 0, 0);
  for (size_t i = 0; i < rows.size(); i++) {
    Row r = rows[i];
    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO CIRCUIT VALUES('%s','%s',%d,%d,%d,%d,%d,%d)",
             esc(r["CIRCUIT_ID"]).c_str(), esc(r["CIRCUIT_NM"]).c_str(),
             to_int(r["A_ASSET_ID"]), to_int(r["Z_ASSET_ID"]),
             to_int(r["CAP_MBPS"]), to_int(r["ALLOC_MBPS"]),
             to_int(r["ROLE_CD"]), to_int(r["STATUS_CD"]));
    sqlite3_exec(db, sql, 0, 0, 0);
  }
  sqlite3_exec(db, "COMMIT", 0, 0, 0);
}

std::vector<Row> Store::query(const std::string &sql, const std::vector<std::string> &params) {
  std::vector<Row> out;
  sqlite3_stmt *st = 0;
  const char *tail = 0;
  if (sqlite3_prepare_v2(db, sql.c_str(), (int)sql.size() + 1, &st, &tail) != SQLITE_OK || !st) {
    fprintf(stderr, "query failed: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(st);
    return out;
  }
  for (const char *t = tail; t && *t; t++) {
    if (*t != ' ' && *t != '\t' && *t != '\n' && *t != '\r') {
      fprintf(stderr, "query failed: more than one statement\n");
      sqlite3_finalize(st);
      return out;
    }
  }
  if (sqlite3_bind_parameter_count(st) != (int)params.size()) {
    fprintf(stderr, "query failed: expected %d params, got %d\n",
            sqlite3_bind_parameter_count(st), (int)params.size());
    sqlite3_finalize(st);
    return out;
  }
  for (size_t i = 0; i < params.size(); i++)
    sqlite3_bind_text(st, (int)i + 1, params[i].c_str(), (int)params[i].size(), SQLITE_TRANSIENT);
  int rc;
  while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
    Row r;
    int n = sqlite3_column_count(st);
    for (int c = 0; c < n; c++) {
      const unsigned char *v = sqlite3_column_text(st, c);
      r[sqlite3_column_name(st, c)] = v ? (const char *)v : "";
    }
    out.push_back(r);
  }
  if (rc != SQLITE_DONE) fprintf(stderr, "query failed: %s\n", sqlite3_errmsg(db));
  sqlite3_finalize(st);
  return out;
}
