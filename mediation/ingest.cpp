#include <cstdio>
#include <string>
#include <string_view>
#include "store.h"
#include "capacity.h"
#include "status.h"
#include "circuit_counter.h"

/* nightly mediation run. reads the element telemetry drops out of /data and
   rebuilds the local store. cron: 0 2 * * * */

int main(int argc, char **argv) {
  std::string data = "data";
  std::string dbpath = "meridian.db";
  for (int i = 1; i < argc; i++) {
    const std::string_view a = argv[i];
    if (a == "--data" && i + 1 < argc) data = argv[++i];
    else if (a == "--db" && i + 1 < argc) dbpath = argv[++i];
  }

  const std::vector<Row> sites = read_csv(data + "/sites.csv");
  const std::vector<Row> circuits = read_csv(data + "/circuits.csv");
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
  for (const Row &r : sites) {
    if (status_is_countable(to_int(field(r, "STATUS_CD")))) countable++;
    total += to_int(field(r, "TOTAL_CAP_MBPS"));
    alloc += to_int(field(r, "ALLOC_CAP_MBPS"));
  }

  printf("mediation run complete\n");
  printf("  sites loaded      : %d\n", static_cast<int>(sites.size()));
  printf("  countable sites   : %d\n", countable);
  printf("  circuits loaded   : %d\n", static_cast<int>(circuits.size()));
  printf("  active circuits   : %d\n", count_active_circuits(circuits));
  printf("  active capacity   : %d mbps\n", sum_active_capacity(circuits));
  printf("  available capacity: %d mbps\n", available_capacity(static_cast<int>(total), static_cast<int>(alloc)));
  printf("  utilization       : %d%%\n", utilization_pct(static_cast<int>(total), static_cast<int>(alloc)));
  return 0;
}
