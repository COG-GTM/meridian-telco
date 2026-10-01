#include <cstdio>
#include <string>
#include <vector>
#include "store.h"

/* store query checks. bin/store-test, run with make check. */

static int failures = 0;

static void check_true(const char *what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static size_t count(Store &s, const char *table) {
  return s.query(std::string("SELECT * FROM ") + table).size();
}

int main() {
  Store s;
  if (!s.open(":memory:")) return 1;
  s.create_schema();
  std::vector<Row> sites(2), circuits(1);
  sites[0]["ASSET_ID"] = "1"; sites[0]["REGION_CD"] = "NE-04"; sites[0]["STATUS_CD"] = "1";
  sites[1]["ASSET_ID"] = "2"; sites[1]["REGION_CD"] = "MW-07"; sites[1]["STATUS_CD"] = "2";
  circuits[0]["CIRCUIT_ID"] = "CKT-1";
  s.load_sites(sites);
  s.load_circuits(circuits);

  std::vector<std::string> p(1, "NE-04");
  check_true("bound REGION_CD matches one site",
             s.query("SELECT * FROM SITE WHERE REGION_CD=?", p).size() == 1);

  p[0] = "x' OR '1'='1";
  check_true("quote payload in REGION_CD matches nothing",
             s.query("SELECT * FROM SITE WHERE REGION_CD=?", p).empty());

  p[0] = "x'; DROP TABLE SITE;--";
  s.query("SELECT * FROM SITE WHERE REGION_CD=?", p);
  check_true("stacked DROP in REGION_CD leaves SITE intact", count(s, "SITE") == 2);

  p[0] = "x' UNION SELECT * FROM CIRCUIT--";
  check_true("UNION payload in CIRCUIT_ID matches nothing",
             s.query("SELECT * FROM CIRCUIT WHERE CIRCUIT_ID=?", p).empty());

  p[0] = "2";
  check_true("bound STATUS_CD compares as integer",
             s.query("SELECT * FROM SITE WHERE STATUS_CD=CAST(? AS INTEGER)", p).size() == 1);

  s.query("SELECT * FROM SITE; DELETE FROM SITE");
  check_true("multi-statement sql is refused", count(s, "SITE") == 2);

  check_true("param count mismatch is refused",
             s.query("SELECT * FROM SITE WHERE REGION_CD=?").empty());

  printf("%s\n", failures ? "FAILED" : "all store checks passed");
  return failures ? 1 : 0;
}
