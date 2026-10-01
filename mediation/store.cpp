#include "store.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

Store::Store() : db(0) {}

Store::~Store() { close(); }

#ifdef MERIDIAN_SQLCIPHER
static const size_t MIN_KEY_BYTES = 32;

static void wipe(std::string &s) {
  std::fill(s.begin(), s.end(), '\0');
  s.clear();
}

/* the store key comes from the secret store, mounted as a file
   (MERIDIAN_DB_KEY_FILE) or injected as MERIDIAN_DB_KEY. */
static bool load_key(std::string &key) {
  const char *file = getenv("MERIDIAN_DB_KEY_FILE");
  const char *env = getenv("MERIDIAN_DB_KEY");
  if (file && *file) {
    FILE *f = fopen(file, "rb");
    if (!f) {
      fprintf(stderr, "cannot read MERIDIAN_DB_KEY_FILE %s: %s\n", file, strerror(errno));
      return false;
    }
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    key.assign(buf, n);
    memset(buf, 0, sizeof(buf));
    while (!key.empty() && (key[key.size() - 1] == '\n' || key[key.size() - 1] == '\r'))
      key.erase(key.size() - 1);
  } else if (env) {
    key = env;
  }
  if (key.size() < MIN_KEY_BYTES) {
    fprintf(stderr, "store key missing or shorter than %d bytes: set MERIDIAN_DB_KEY_FILE "
                    "(preferred) or MERIDIAN_DB_KEY\n", (int)MIN_KEY_BYTES);
    wipe(key);
    return false;
  }
  return true;
}

static int first_column(void *ctx, int argc, char **argv, char **) {
  if (argc > 0 && argv[0]) *(std::string *)ctx = argv[0];
  return 0;
}

static bool apply_key(sqlite3 *db) {
  std::string version;
  sqlite3_exec(db, "PRAGMA cipher_version", first_column, &version, 0);
  if (version.empty()) {
    fprintf(stderr, "store library is not SQLCipher, refusing to open an unencrypted store\n");
    return false;
  }
  std::string key;
  if (!load_key(key)) return false;
  int rc = sqlite3_key(db, key.data(), (int)key.size());
  wipe(key);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "cannot key store: %s\n", sqlite3_errmsg(db));
    return false;
  }
  return true;
}
#endif

bool Store::open(const std::string &path) {
  int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
  if (fd < 0) {
    fprintf(stderr, "cannot open store %s: %s\n", path.c_str(), strerror(errno));
    return false;
  }
  ::close(fd);
  if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
    fprintf(stderr, "cannot open store %s\n", path.c_str());
    close();
    return false;
  }
#ifdef MERIDIAN_SQLCIPHER
  if (!apply_key(db)) {
    close();
    return false;
  }
#else
  fprintf(stderr, "warning: store %s is not encrypted at rest (built with SQLCIPHER=0)\n",
          path.c_str());
#endif
  if (sqlite3_exec(db, "SELECT count(*) FROM sqlite_master", 0, 0, 0) != SQLITE_OK) {
    fprintf(stderr, "cannot read store %s: %s (wrong key, or a plaintext store from an older "
                    "build: remove it and rerun mediation)\n", path.c_str(), sqlite3_errmsg(db));
    close();
    return false;
  }
  if (sqlite3_exec(db, "PRAGMA synchronous=FULL", 0, 0, 0) != SQLITE_OK) {
    fprintf(stderr, "cannot set synchronous=FULL on store %s: %s\n", path.c_str(),
            sqlite3_errmsg(db));
    close();
    return false;
  }
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
