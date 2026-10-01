#include <cstdio>
#include <cstdlib>
#include <string>
#include "store.h"
#include "capacity.h"
#include "status.h"
#include "circuit_counter.h"

/* nightly mediation run. reads the element telemetry drops out of /data and
   applies them to the local store as a snapshot; each run and every row it
   changes is recorded in INGEST_RUN / INGEST_AUDIT. cron: 0 2 * * * */

int main(int argc, char **argv) {
  std::string data = "data";
  std::string dbpath = "meridian.db";
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--data" && i + 1 < argc) data = argv[++i];
    else if (a == "--db" && i + 1 < argc) dbpath = argv[++i];
  }

  std::vector<Row> sites = read_csv(data + "/sites.csv");
  std::vector<Row> circuits = read_csv(data + "/circuits.csv");
  if (sites.empty()) {
    fprintf(stderr, "no site telemetry found in %s\n", data.c_str());
    return 1;
  }

  Store st;
  if (!st.open(dbpath)) return 1;
  st.create_schema();
  IngestStats run;
  std::string source = "sites=" + data + "/sites.csv circuits=" + data + "/circuits.csv";
  if (!st.ingest(sites, circuits, source, &run)) {
    fprintf(stderr, "ingest run %ld failed, store left unchanged: %s\n", run.run_id,
            st.last_error().c_str());
    return 1;
  }

  int countable = 0;
  long total = 0, alloc = 0;
  for (size_t i = 0; i < sites.size(); i++) {
    Row r = sites[i];
    if (status_is_countable(to_int(r["STATUS_CD"]))) countable++;
    total += to_int(r["TOTAL_CAP_MBPS"]);
    alloc += to_int(r["ALLOC_CAP_MBPS"]);
  }

  printf("mediation run complete\n");
  printf("  ingest run        : %ld\n", run.run_id);
  printf("  rows inserted     : %d\n", run.inserted);
  printf("  rows updated      : %d\n", run.updated);
  printf("  rows deleted      : %d\n", run.deleted);
  if (run.dup_keys) printf("  duplicate keys    : %d (last row kept)\n", run.dup_keys);
  printf("  sites loaded      : %d\n", (int)sites.size());
  printf("  countable sites   : %d\n", countable);
  printf("  circuits loaded   : %d\n", (int)circuits.size());
  printf("  active circuits   : %d\n", count_active_circuits(circuits));
  printf("  active capacity   : %d mbps\n", sum_active_capacity(circuits));
  printf("  available capacity: %d mbps\n", available_capacity((int)total, (int)alloc));
  printf("  utilization       : %d%%\n", utilization_pct((int)total, (int)alloc));
  return 0;
}
