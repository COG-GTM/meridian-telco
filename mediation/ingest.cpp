#include <cstdio>
#include <cstdlib>
#include <string>
#include "store.h"
#include "status.h"
#include "circuit_counter.h"
#include "unified_inventory_rules/capacity.h"

/* nightly mediation run. reads the element telemetry drops out of /data and
   rebuilds the local store. cron: 0 2 * * * */

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
  st.load_sites(sites);
  st.load_circuits(circuits);

  int countable = 0;
  long total = 0, alloc = 0;
  for (size_t i = 0; i < sites.size(); i++) {
    Row r = sites[i];
    if (status_is_countable(to_int(r["STATUS_CD"]))) countable++;
    total += to_int(r["TOTAL_CAP_MBPS"]);
    alloc += to_int(r["ALLOC_CAP_MBPS"]);
  }

  printf("mediation run complete\n");
  printf("  sites loaded      : %d\n", (int)sites.size());
  printf("  countable sites   : %d\n", countable);
  printf("  circuits loaded   : %d\n", (int)circuits.size());
  printf("  active circuits   : %d\n", count_active_circuits(circuits));
  printf("  active capacity   : %d mbps\n", sum_active_capacity(circuits));
  printf("  available capacity: %d mbps\n",
         unified_inventory_rules::available_capacity((int)total, (int)alloc));
  printf("  utilization       : %.2f%%\n",
         unified_inventory_rules::utilization_pct((int)total, (int)alloc));
  return 0;
}
