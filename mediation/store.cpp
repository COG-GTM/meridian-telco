#include "store.h"
#include <cstdio>

bool Store::open(const std::string &path) {
  sqlite3 *raw = nullptr;
  const int rc = sqlite3_open(path.c_str(), &raw);
  db_.reset(raw);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "cannot open store %s\n", path.c_str());
    return false;
  }
  exec("PRAGMA synchronous=OFF");
  return true;
}

void Store::close() { db_.reset(); }

bool Store::exec(const std::string &sql) {
  return sqlite3_exec(db_.get(), sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

SqliteStmt Store::prepare(std::string_view sql) {
  sqlite3_stmt *raw = nullptr;
  sqlite3_prepare_v2(db_.get(), sql.data(), static_cast<int>(sql.size()), &raw, nullptr);
  return SqliteStmt(raw);
}

void Store::create_schema() {
  const std::string ddl =
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
  if (!exec(ddl)) fprintf(stderr, "ddl failed: %s\n", sqlite3_errmsg(db_.get()));
}

namespace {

std::string column_text(sqlite3_stmt *stmt, int column) {
  const auto *text = sqlite3_column_text(stmt, column);
  if (!text) return {};
  return std::string(text, text + sqlite3_column_bytes(stmt, column));
}

void bind_text(sqlite3_stmt *stmt, int idx, std::string_view s) {
  sqlite3_bind_text(stmt, idx, s.empty() ? "" : s.data(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
}

/* coordinates were historically written as %f literals (6 decimals); bind the
   same text and let the REAL column affinity convert it exactly as before */
void bind_fixed6(sqlite3_stmt *stmt, int idx, double v) { bind_text(stmt, idx, std::to_string(v)); }

void step_and_reset(sqlite3_stmt *stmt) {
  sqlite3_step(stmt);
  sqlite3_reset(stmt);
  sqlite3_clear_bindings(stmt);
}

}  // namespace

void Store::load_sites(const std::vector<Row> &rows) {
  exec("BEGIN");
  if (SqliteStmt stmt = prepare("INSERT OR REPLACE INTO SITE VALUES(?,?,?,?,?,?,?,?,?,?)")) {
    for (const Row &r : rows) {
      sqlite3_stmt *s = stmt.get();
      sqlite3_bind_int(s, 1, to_int(field(r, "ASSET_ID")));
      bind_text(s, 2, field(r, "SITE_NM"));
      bind_text(s, 3, field(r, "SITE_CD"));
      bind_text(s, 4, field(r, "REGION_CD"));
      bind_fixed6(s, 5, to_dbl(field(r, "LAT")));
      bind_fixed6(s, 6, to_dbl(field(r, "LON")));
      sqlite3_bind_int(s, 7, to_int(field(r, "STATUS_CD")));
      bind_text(s, 8, field(r, "TOWER_REG"));
      sqlite3_bind_int(s, 9, to_int(field(r, "TOTAL_CAP_MBPS")));
      sqlite3_bind_int(s, 10, to_int(field(r, "ALLOC_CAP_MBPS")));
      step_and_reset(s);
    }
  }
  exec("COMMIT");
}

void Store::load_circuits(const std::vector<Row> &rows) {
  exec("BEGIN");
  if (SqliteStmt stmt = prepare("INSERT OR REPLACE INTO CIRCUIT VALUES(?,?,?,?,?,?,?,?)")) {
    for (const Row &r : rows) {
      sqlite3_stmt *s = stmt.get();
      bind_text(s, 1, field(r, "CIRCUIT_ID"));
      bind_text(s, 2, field(r, "CIRCUIT_NM"));
      sqlite3_bind_int(s, 3, to_int(field(r, "A_ASSET_ID")));
      sqlite3_bind_int(s, 4, to_int(field(r, "Z_ASSET_ID")));
      sqlite3_bind_int(s, 5, to_int(field(r, "CAP_MBPS")));
      sqlite3_bind_int(s, 6, to_int(field(r, "ALLOC_MBPS")));
      sqlite3_bind_int(s, 7, to_int(field(r, "ROLE_CD")));
      sqlite3_bind_int(s, 8, to_int(field(r, "STATUS_CD")));
      step_and_reset(s);
    }
  }
  exec("COMMIT");
}

std::vector<Row> Store::query(const std::string &sql) {
  std::vector<Row> out;
  std::string_view rest = sql;
  while (!rest.empty()) {
    sqlite3_stmt *raw = nullptr;
    const char *tail = nullptr;
    const int rc = sqlite3_prepare_v2(db_.get(), rest.data(), static_cast<int>(rest.size()), &raw, &tail);
    const SqliteStmt stmt(raw);
    if (rc != SQLITE_OK) {
      fprintf(stderr, "query failed: %s\n", sqlite3_errmsg(db_.get()));
      return out;
    }
    rest.remove_prefix(static_cast<size_t>(tail - rest.data()));
    if (!stmt) continue;
    int step;
    while ((step = sqlite3_step(stmt.get())) == SQLITE_ROW) {
      Row r;
      const int ncol = sqlite3_column_count(stmt.get());
      for (int i = 0; i < ncol; i++) {
        r[sqlite3_column_name(stmt.get(), i)] = column_text(stmt.get(), i);
      }
      out.push_back(std::move(r));
    }
    if (step != SQLITE_DONE) {
      fprintf(stderr, "query failed: %s\n", sqlite3_errmsg(db_.get()));
      return out;
    }
  }
  return out;
}
