#ifndef MERIDIAN_STORE_H
#define MERIDIAN_STORE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "csv.h"

/* the local mediation store. one sqlite file, lives next to the binary. */
class Store {
 public:
  Store();
  ~Store();
  bool open(const std::string &path);
  void close();
  void create_schema();
  void load_sites(const std::vector<Row> &rows);
  void load_circuits(const std::vector<Row> &rows);
  /* one statement, values bound to ? placeholders in order. */
  std::vector<Row> query(const std::string &sql,
                         const std::vector<std::string> &params = std::vector<std::string>());
  sqlite3 *handle() { return db; }

 private:
  sqlite3 *db;
};

#endif
