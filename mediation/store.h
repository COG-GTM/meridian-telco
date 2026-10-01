#ifndef MERIDIAN_STORE_H
#define MERIDIAN_STORE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "csv.h"

struct TableSpec;

struct IngestStats {
  long run_id;
  int rows_read;
  int inserted;
  int updated;
  int deleted;
  int dup_keys;
  IngestStats() : run_id(-1), rows_read(0), inserted(0), updated(0), deleted(0), dup_keys(0) {}
};

/* the local mediation store. one sqlite file, lives next to the binary.

   every ingest is recorded in INGEST_RUN (run id, start/finish, source,
   status, counts) and every row it inserts, changes or drops is written to
   INGEST_AUDIT with the before/after values. both tables are append-only. */
class Store {
 public:
  Store();
  ~Store();
  bool open(const std::string &path);
  void close();
  void create_schema();
  /* applies the site and circuit feeds as one snapshot. returns false and
     leaves SITE/CIRCUIT untouched if anything fails; the run is still
     recorded as FAILED. */
  bool ingest(const std::vector<Row> &sites, const std::vector<Row> &circuits,
              const std::string &source, IngestStats *stats);
  std::vector<Row> query(const std::string &sql);
  const std::string &last_error() const { return err; }
  sqlite3 *handle() { return db; }

 private:
  bool exec(const std::string &sql);
  bool sync_table(const TableSpec &t, const std::vector<Row> &rows, IngestStats *stats);
  sqlite3 *db;
  std::string err;
};

#endif
