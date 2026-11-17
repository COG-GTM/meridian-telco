#ifndef MERIDIAN_STORE_H
#define MERIDIAN_STORE_H

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <sqlite3.h>
#include "csv.h"

struct SqliteCloser {
  void operator()(sqlite3 *db) const noexcept { sqlite3_close(db); }
};
struct SqliteFinalizer {
  void operator()(sqlite3_stmt *stmt) const noexcept { sqlite3_finalize(stmt); }
};
struct SqliteFreer {
  void operator()(void *p) const noexcept { sqlite3_free(p); }
};

using SqliteDb = std::unique_ptr<sqlite3, SqliteCloser>;
using SqliteStmt = std::unique_ptr<sqlite3_stmt, SqliteFinalizer>;
using SqliteErrMsg = std::unique_ptr<char, SqliteFreer>;

/* the local mediation store. one sqlite file, lives next to the binary. */
class Store {
 public:
  Store() = default;
  ~Store() = default;
  Store(const Store &) = delete;
  Store &operator=(const Store &) = delete;
  Store(Store &&) noexcept = default;
  Store &operator=(Store &&) noexcept = default;

  bool open(const std::string &path);
  void close();
  void create_schema();
  void load_sites(const std::vector<Row> &rows);
  void load_circuits(const std::vector<Row> &rows);
  std::vector<Row> query(const std::string &sql);
  sqlite3 *handle() { return db_.get(); }

 private:
  bool exec(const char *sql);
  SqliteStmt prepare(std::string_view sql);
  SqliteDb db_;
};

#endif
