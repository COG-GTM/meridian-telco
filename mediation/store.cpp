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
      "CREATE TABLE IF NOT EXISTS SITE("
      " ASSET_ID INTEGER PRIMARY KEY,"
      " SITE_NM TEXT, SITE_CD TEXT, REGION_CD TEXT,"
      " LAT REAL, LON REAL, STATUS_CD INTEGER, TOWER_REG TEXT,"
      " TOTAL_CAP_MBPS INTEGER, ALLOC_CAP_MBPS INTEGER);"
      "CREATE TABLE IF NOT EXISTS CIRCUIT("
      " CIRCUIT_ID TEXT PRIMARY KEY, CIRCUIT_NM TEXT,"
      " A_ASSET_ID INTEGER, Z_ASSET_ID INTEGER,"
      " CAP_MBPS INTEGER, ALLOC_MBPS INTEGER,"
      " ROLE_CD INTEGER, STATUS_CD INTEGER);"
      "CREATE TABLE IF NOT EXISTS INGEST_RUN("
      " RUN_ID INTEGER PRIMARY KEY AUTOINCREMENT,"
      " STARTED_AT TEXT NOT NULL, FINISHED_AT TEXT,"
      " SOURCE TEXT NOT NULL,"
      " STATUS TEXT NOT NULL CHECK(STATUS IN ('RUNNING','SUCCEEDED','FAILED')),"
      " ROWS_READ INTEGER, INSERTED INTEGER, UPDATED INTEGER, DELETED INTEGER,"
      " DUP_KEYS INTEGER, ERROR TEXT);"
      "CREATE TABLE IF NOT EXISTS INGEST_AUDIT("
      " AUDIT_ID INTEGER PRIMARY KEY AUTOINCREMENT,"
      " RUN_ID INTEGER NOT NULL REFERENCES INGEST_RUN(RUN_ID),"
      " TBL TEXT NOT NULL, ROW_KEY TEXT NOT NULL,"
      " ACTION TEXT NOT NULL CHECK(ACTION IN ('INSERT','UPDATE','DELETE')),"
      " CHANGED_COLS TEXT, BEFORE_JSON TEXT, AFTER_JSON TEXT);"
      "CREATE INDEX IF NOT EXISTS INGEST_AUDIT_ROW ON INGEST_AUDIT(TBL, ROW_KEY);"
      "CREATE INDEX IF NOT EXISTS INGEST_AUDIT_RUN ON INGEST_AUDIT(RUN_ID);"
      "CREATE TRIGGER IF NOT EXISTS INGEST_AUDIT_NO_UPDATE BEFORE UPDATE ON INGEST_AUDIT"
      " BEGIN SELECT RAISE(ABORT, 'INGEST_AUDIT is append-only'); END;"
      "CREATE TRIGGER IF NOT EXISTS INGEST_AUDIT_NO_DELETE BEFORE DELETE ON INGEST_AUDIT"
      " BEGIN SELECT RAISE(ABORT, 'INGEST_AUDIT is append-only'); END;"
      "CREATE TRIGGER IF NOT EXISTS INGEST_RUN_NO_DELETE BEFORE DELETE ON INGEST_RUN"
      " BEGIN SELECT RAISE(ABORT, 'INGEST_RUN is append-only'); END;"
      "CREATE TRIGGER IF NOT EXISTS INGEST_RUN_SEALED BEFORE UPDATE ON INGEST_RUN"
      " WHEN OLD.STATUS <> 'RUNNING'"
      " BEGIN SELECT RAISE(ABORT, 'INGEST_RUN row is sealed'); END;";
  if (!exec(ddl)) fprintf(stderr, "ddl failed: %s\n", err.c_str());
}

bool Store::exec(const std::string &sql) {
  char *e = 0;
  if (sqlite3_exec(db, sql.c_str(), 0, 0, &e) != SQLITE_OK) {
    err = e ? e : sqlite3_errmsg(db);
    if (e) sqlite3_free(e);
    return false;
  }
  return true;
}

struct Column {
  const char *name;
  char kind; /* I integer, R real, T text */
};

struct TableSpec {
  const char *name;
  const Column *cols;
  int ncols; /* cols[0] is the primary key */
};

static const Column SITE_COLS[] = {
    {"ASSET_ID", 'I'}, {"SITE_NM", 'T'}, {"SITE_CD", 'T'}, {"REGION_CD", 'T'},
    {"LAT", 'R'}, {"LON", 'R'}, {"STATUS_CD", 'I'}, {"TOWER_REG", 'T'},
    {"TOTAL_CAP_MBPS", 'I'}, {"ALLOC_CAP_MBPS", 'I'}};
static const Column CIRCUIT_COLS[] = {
    {"CIRCUIT_ID", 'T'}, {"CIRCUIT_NM", 'T'}, {"A_ASSET_ID", 'I'}, {"Z_ASSET_ID", 'I'},
    {"CAP_MBPS", 'I'}, {"ALLOC_MBPS", 'I'}, {"ROLE_CD", 'I'}, {"STATUS_CD", 'I'}};
static const TableSpec SITE_SPEC = {"SITE", SITE_COLS, 10};
static const TableSpec CIRCUIT_SPEC = {"CIRCUIT", CIRCUIT_COLS, 8};

static std::string field(const Row &r, const char *name) {
  Row::const_iterator it = r.find(name);
  return it == r.end() ? std::string() : it->second;
}

static std::string col_list(const TableSpec &t, const std::string &prefix) {
  std::string o;
  for (int i = 0; i < t.ncols; i++) {
    if (i) o += ",";
    o += prefix + t.cols[i].name;
  }
  return o;
}

static std::string json_of(const TableSpec &t, const std::string &prefix) {
  std::string o = "json_object(";
  for (int i = 0; i < t.ncols; i++) {
    if (i) o += ",";
    o += std::string("'") + t.cols[i].name + "'," + prefix + t.cols[i].name;
  }
  return o + ")";
}

static std::string differs(const TableSpec &t, const std::string &a, const std::string &b) {
  std::string o;
  for (int i = 1; i < t.ncols; i++) {
    if (i > 1) o += " OR ";
    o += a + t.cols[i].name + " IS NOT " + b + t.cols[i].name;
  }
  return "(" + o + ")";
}

static std::string changed_cols(const TableSpec &t) {
  std::string o;
  for (int i = 1; i < t.ncols; i++) {
    if (i > 1) o += "||";
    o += std::string("CASE WHEN t.") + t.cols[i].name + " IS NOT s." + t.cols[i].name +
         " THEN '" + t.cols[i].name + ",' ELSE '' END";
  }
  return "rtrim(" + o + ",',')";
}

/* stages the feed, records the row-level diff against the live table in
   INGEST_AUDIT, then applies it. runs inside the caller's transaction. */
bool Store::sync_table(const TableSpec &t, const std::vector<Row> &rows, IngestStats *stats) {
  std::string tbl = t.name;
  std::string stg = "STG_" + tbl;
  std::string key = t.cols[0].name;
  char run[32];
  snprintf(run, sizeof(run), "%ld", stats->run_id);

  std::string ddl = "DROP TABLE IF EXISTS temp." + stg + "; CREATE TEMP TABLE " + stg + "(";
  for (int i = 0; i < t.ncols; i++) {
    if (i) ddl += ",";
    ddl += t.cols[i].name;
    ddl += t.cols[i].kind == 'I' ? " INTEGER" : t.cols[i].kind == 'R' ? " REAL" : " TEXT";
    if (i == 0) ddl += " PRIMARY KEY";
  }
  ddl += ")";
  if (!exec(ddl)) return false;

  std::string ins = "INSERT OR REPLACE INTO " + stg + " VALUES(";
  for (int i = 0; i < t.ncols; i++) ins += i ? ",?" : "?";
  ins += ")";
  sqlite3_stmt *st = 0;
  if (sqlite3_prepare_v2(db, ins.c_str(), -1, &st, 0) != SQLITE_OK) {
    err = sqlite3_errmsg(db);
    return false;
  }
  for (size_t r = 0; r < rows.size(); r++) {
    sqlite3_reset(st);
    for (int i = 0; i < t.ncols; i++) {
      std::string v = field(rows[r], t.cols[i].name);
      if (t.cols[i].kind == 'I') sqlite3_bind_int(st, i + 1, to_int(v));
      else if (t.cols[i].kind == 'R') {
        /* bound as %f text so sqlite parses it into the REAL column exactly as
           earlier loads did; keeps stored coordinates stable across runs */
        char num[64];
        snprintf(num, sizeof(num), "%f", to_dbl(v));
        sqlite3_bind_text(st, i + 1, num, -1, SQLITE_TRANSIENT);
      }
      else sqlite3_bind_text(st, i + 1, v.c_str(), -1, SQLITE_TRANSIENT);
    }
    if (sqlite3_step(st) != SQLITE_DONE) {
      err = sqlite3_errmsg(db);
      sqlite3_finalize(st);
      return false;
    }
  }
  sqlite3_finalize(st);

  std::vector<Row> n = query("SELECT COUNT(*) AS N FROM " + stg);
  int staged = n.empty() ? 0 : to_int(n[0]["N"]);
  stats->rows_read += (int)rows.size();
  stats->dup_keys += (int)rows.size() - staged;

  std::string audit = "INSERT INTO INGEST_AUDIT(RUN_ID,TBL,ROW_KEY,ACTION,CHANGED_COLS,BEFORE_JSON,AFTER_JSON) ";
  if (!exec(audit + "SELECT " + run + ",'" + tbl + "',s." + key + ",'INSERT',NULL,NULL," +
            json_of(t, "s.") + " FROM " + stg + " s WHERE NOT EXISTS (SELECT 1 FROM " + tbl +
            " t WHERE t." + key + "=s." + key + ") ORDER BY s." + key))
    return false;
  stats->inserted += sqlite3_changes(db);
  if (!exec(audit + "SELECT " + run + ",'" + tbl + "',s." + key + ",'UPDATE'," + changed_cols(t) +
            "," + json_of(t, "t.") + "," + json_of(t, "s.") + " FROM " + stg + " s JOIN " + tbl +
            " t ON t." + key + "=s." + key + " WHERE " + differs(t, "t.", "s.") + " ORDER BY s." + key))
    return false;
  stats->updated += sqlite3_changes(db);
  if (!exec(audit + "SELECT " + run + ",'" + tbl + "',t." + key + ",'DELETE',NULL," +
            json_of(t, "t.") + ",NULL FROM " + tbl + " t WHERE NOT EXISTS (SELECT 1 FROM " + stg +
            " s WHERE s." + key + "=t." + key + ") ORDER BY t." + key))
    return false;
  stats->deleted += sqlite3_changes(db);

  std::string set;
  for (int i = 1; i < t.ncols; i++) {
    if (i > 1) set += ",";
    set += std::string(t.cols[i].name) + "=excluded." + t.cols[i].name;
  }
  return exec("DELETE FROM " + tbl + " WHERE NOT EXISTS (SELECT 1 FROM " + stg + " s WHERE s." +
              key + "=" + tbl + "." + key + ")") &&
         exec("INSERT INTO " + tbl + "(" + col_list(t, "") + ") SELECT " + col_list(t, "") +
              " FROM " + stg + " WHERE true ON CONFLICT(" + key + ") DO UPDATE SET " + set +
              " WHERE " + differs(t, tbl + ".", "excluded.")) &&
         exec("DROP TABLE temp." + stg);
}

bool Store::ingest(const std::vector<Row> &sites, const std::vector<Row> &circuits,
                   const std::string &source, IngestStats *stats) {
  *stats = IngestStats();
  err.clear();
  sqlite3_stmt *st = 0;
  if (sqlite3_prepare_v2(db,
                         "INSERT INTO INGEST_RUN(STARTED_AT,SOURCE,STATUS)"
                         " VALUES(strftime('%Y-%m-%dT%H:%M:%fZ','now'),?,'RUNNING')",
                         -1, &st, 0) != SQLITE_OK) {
    err = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_text(st, 1, source.c_str(), -1, SQLITE_TRANSIENT);
  bool started = sqlite3_step(st) == SQLITE_DONE;
  if (!started) err = sqlite3_errmsg(db);
  sqlite3_finalize(st);
  if (!started) return false;
  stats->run_id = (long)sqlite3_last_insert_rowid(db);

  bool ok = exec("BEGIN IMMEDIATE");
  if (ok) {
    ok = sync_table(SITE_SPEC, sites, stats) && sync_table(CIRCUIT_SPEC, circuits, stats) &&
         exec("COMMIT");
    if (!ok) {
      std::string cause = err;
      exec("ROLLBACK");
      err = cause;
    }
  }
  if (!ok) {
    IngestStats failed;
    failed.run_id = stats->run_id;
    failed.rows_read = stats->rows_read;
    *stats = failed;
  }

  if (sqlite3_prepare_v2(db,
                         "UPDATE INGEST_RUN SET FINISHED_AT=strftime('%Y-%m-%dT%H:%M:%fZ','now'),"
                         " STATUS=?, ROWS_READ=?, INSERTED=?, UPDATED=?, DELETED=?, DUP_KEYS=?,"
                         " ERROR=? WHERE RUN_ID=?",
                         -1, &st, 0) != SQLITE_OK) {
    if (ok) err = sqlite3_errmsg(db);
    return false;
  }
  sqlite3_bind_text(st, 1, ok ? "SUCCEEDED" : "FAILED", -1, SQLITE_STATIC);
  sqlite3_bind_int(st, 2, stats->rows_read);
  sqlite3_bind_int(st, 3, stats->inserted);
  sqlite3_bind_int(st, 4, stats->updated);
  sqlite3_bind_int(st, 5, stats->deleted);
  sqlite3_bind_int(st, 6, stats->dup_keys);
  if (ok) sqlite3_bind_null(st, 7);
  else sqlite3_bind_text(st, 7, err.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(st, 8, stats->run_id);
  bool sealed = sqlite3_step(st) == SQLITE_DONE;
  if (!sealed && ok) err = sqlite3_errmsg(db);
  sqlite3_finalize(st);
  return ok && sealed;
}

static int collect(void *ctx, int argc, char **argv, char **cols) {
  std::vector<Row> *out = (std::vector<Row> *)ctx;
  Row r;
  for (int i = 0; i < argc; i++) r[cols[i]] = argv[i] ? argv[i] : "";
  out->push_back(r);
  return 0;
}

std::vector<Row> Store::query(const std::string &sql) {
  std::vector<Row> out;
  char *err = 0;
  if (sqlite3_exec(db, sql.c_str(), collect, &out, &err) != SQLITE_OK) {
    fprintf(stderr, "query failed: %s\n", err ? err : "?");
    if (err) sqlite3_free(err);
  }
  return out;
}
