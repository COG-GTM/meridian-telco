#include <cstdio>
#include <string>
#include <vector>
#include "store.h"

/* ingest audit trail checks. bin/store-test, run with make check. */

static int failures = 0;

static void check_true(const char *what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what);
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static void check_str(const char *what, const std::string &got, const std::string &want) {
  if (got != want) {
    printf("FAIL %-52s got '%s' want '%s'\n", what, got.c_str(), want.c_str());
    failures++;
  } else {
    printf("ok   %s\n", what);
  }
}

static Row site(const char *id, const char *name, const char *total, const char *alloc) {
  Row r;
  r["ASSET_ID"] = id;
  r["SITE_NM"] = name;
  r["SITE_CD"] = std::string("MSITE-") + id;
  r["REGION_CD"] = "NE-04";
  r["LAT"] = "43.1";
  r["LON"] = "-71.3";
  r["STATUS_CD"] = "1";
  r["TOWER_REG"] = "TWR-NE-0001";
  r["TOTAL_CAP_MBPS"] = total;
  r["ALLOC_CAP_MBPS"] = alloc;
  return r;
}

static Row circuit(const char *id, const char *alloc) {
  Row r;
  r["CIRCUIT_ID"] = id;
  r["CIRCUIT_NM"] = "O'Brien Ring";
  r["A_ASSET_ID"] = "100001";
  r["Z_ASSET_ID"] = "100002";
  r["CAP_MBPS"] = "1000";
  r["ALLOC_MBPS"] = alloc;
  r["ROLE_CD"] = "1";
  r["STATUS_CD"] = "1";
  return r;
}

static std::string one(Store &st, const std::string &sql) {
  std::vector<Row> rows = st.query(sql);
  if (rows.empty() || rows[0].empty()) return "<none>";
  return rows[0].begin()->second;
}

static bool rejected(Store &st, const char *sql) {
  return sqlite3_exec(st.handle(), sql, 0, 0, 0) != SQLITE_OK;
}

int main() {
  Store st;
  if (!st.open(":memory:")) return 1;
  st.create_schema();

  std::vector<Row> sites, circuits;
  sites.push_back(site("100001", "Riverbend", "10000", "4300"));
  sites.push_back(site("100002", "Lakeshore", "2000", "1350"));
  circuits.push_back(circuit("CKT-1", "600"));
  IngestStats s1;
  check_true("first ingest succeeds", st.ingest(sites, circuits, "sites=a.csv circuits=b.csv", &s1));
  check_true("first ingest inserts every row", s1.inserted == 3 && s1.updated == 0 && s1.deleted == 0);
  check_str("run is recorded with its source",
            one(st, "SELECT STATUS||' '||SOURCE FROM INGEST_RUN WHERE RUN_ID=1"),
            "SUCCEEDED sites=a.csv circuits=b.csv");
  check_true("run has start and finish times",
             one(st, "SELECT COUNT(*) FROM INGEST_RUN WHERE STARTED_AT<>'' AND FINISHED_AT>=STARTED_AT") == "1");
  check_str("insert audit carries the new row",
            one(st, "SELECT json_extract(AFTER_JSON,'$.CIRCUIT_NM') FROM INGEST_AUDIT"
                    " WHERE TBL='CIRCUIT' AND ROW_KEY='CKT-1' AND ACTION='INSERT'"),
            "O'Brien Ring");

  sites.clear();
  sites.push_back(site("100001", "Riverbend", "10000", "5100"));
  sites.push_back(site("100003", "Hilltop", "1000", "0"));
  IngestStats s2;
  check_true("second ingest succeeds", st.ingest(sites, circuits, "feed-2", &s2));
  check_true("second ingest counts the diff", s2.inserted == 1 && s2.updated == 1 && s2.deleted == 1);
  check_str("update names the changed column",
            one(st, "SELECT CHANGED_COLS FROM INGEST_AUDIT WHERE RUN_ID=2 AND ROW_KEY='100001'"),
            "ALLOC_CAP_MBPS");
  check_str("update keeps the prior value",
            one(st, "SELECT json_extract(BEFORE_JSON,'$.ALLOC_CAP_MBPS')||'->'||"
                    "json_extract(AFTER_JSON,'$.ALLOC_CAP_MBPS') FROM INGEST_AUDIT"
                    " WHERE RUN_ID=2 AND ROW_KEY='100001'"),
            "4300->5100");
  check_str("dropped site is recoverable from the audit",
            one(st, "SELECT json_extract(BEFORE_JSON,'$.SITE_NM') FROM INGEST_AUDIT"
                    " WHERE RUN_ID=2 AND ACTION='DELETE' AND ROW_KEY='100002'"),
            "Lakeshore");
  check_str("unchanged circuit is not audited",
            one(st, "SELECT COUNT(*) FROM INGEST_AUDIT WHERE RUN_ID=2 AND TBL='CIRCUIT'"), "0");
  check_str("live table reflects the latest feed",
            one(st, "SELECT group_concat(ASSET_ID||':'||ALLOC_CAP_MBPS) FROM"
                    " (SELECT * FROM SITE ORDER BY ASSET_ID)"),
            "100001:5100,100003:0");

  IngestStats s3;
  check_true("identical re-ingest succeeds", st.ingest(sites, circuits, "feed-3", &s3));
  check_true("identical re-ingest changes nothing",
             s3.inserted == 0 && s3.updated == 0 && s3.deleted == 0 &&
             one(st, "SELECT COUNT(*) FROM INGEST_AUDIT WHERE RUN_ID=3") == "0");

  std::vector<Row> dup = sites;
  dup.push_back(site("100003", "Hilltop", "1000", "200"));
  IngestStats s4;
  check_true("duplicate keys in the feed are counted", st.ingest(dup, circuits, "feed-4", &s4) &&
                                                        s4.dup_keys == 1 && s4.updated == 1);

  sqlite3_exec(st.handle(),
               "CREATE TRIGGER BOOM BEFORE INSERT ON SITE BEGIN SELECT RAISE(ABORT,'boom'); END;",
               0, 0, 0);
  sites.push_back(site("100004", "Quarry", "500", "0"));
  IngestStats s5;
  check_true("failing ingest reports failure", !st.ingest(sites, circuits, "feed-5", &s5));
  check_str("failed run is recorded with the error",
            one(st, "SELECT STATUS||' '||ERROR FROM INGEST_RUN WHERE RUN_ID=5"), "FAILED boom");
  check_str("failed run leaves the store and audit untouched",
            one(st, "SELECT (SELECT COUNT(*) FROM SITE)||'/'||"
                    "(SELECT COUNT(*) FROM INGEST_AUDIT WHERE RUN_ID=5)"),
            "2/0");

  check_true("audit rows cannot be edited", rejected(st, "UPDATE INGEST_AUDIT SET ACTION='INSERT'"));
  check_true("audit rows cannot be deleted", rejected(st, "DELETE FROM INGEST_AUDIT"));
  check_true("runs cannot be deleted", rejected(st, "DELETE FROM INGEST_RUN"));
  check_true("finished runs are sealed", rejected(st, "UPDATE INGEST_RUN SET SOURCE='x' WHERE RUN_ID=1"));

  st.close();
  printf("\n%s\n", failures ? "store checks FAILED" : "all store checks passed");
  return failures ? 1 : 0;
}
