#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "capacity.h"
#include "locations.h"

/* shared capacity rule checks. bin/capacity-test <vectors.json>, run with make check.
   the vectors are the golden cases from telco-capacity-rules/spec/vectors.json, the
   same file the vantage pytest suite runs. */

struct Vector {
  std::string name;
  int total, allocated, buffer, available;
  double utilization;
};

static int failures = 0;

/* the vectors file is a flat array of flat objects, no nesting */
static std::string field(const std::string &obj, const std::string &key) {
  std::string pat = "\"" + key + "\"";
  size_t k = obj.find(pat);
  if (k == std::string::npos) return "";
  size_t p = obj.find(':', k + pat.size()) + 1;
  while (p < obj.size() && (obj[p] == ' ' || obj[p] == '"')) p++;
  size_t e = p;
  while (e < obj.size() && obj[e] != ',' && obj[e] != '"' && obj[e] != '}') e++;
  return obj.substr(p, e - p);
}

static std::vector<Vector> load_vectors(const char *path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  std::string s = ss.str();
  std::vector<Vector> out;
  size_t pos = 0;
  while ((pos = s.find('{', pos)) != std::string::npos) {
    size_t end = s.find('}', pos);
    std::string obj = s.substr(pos, end - pos + 1);
    Vector v;
    v.name = field(obj, "name");
    v.total = atoi(field(obj, "total").c_str());
    v.allocated = atoi(field(obj, "allocated").c_str());
    v.buffer = atoi(field(obj, "buffer").c_str());
    v.available = atoi(field(obj, "available").c_str());
    v.utilization = atof(field(obj, "utilization_pct").c_str());
    out.push_back(v);
    pos = end;
  }
  return out;
}

static void check(const std::string &what, bool cond) {
  if (!cond) {
    printf("FAIL %s\n", what.c_str());
    failures++;
  } else {
    printf("ok   %s\n", what.c_str());
  }
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "third_party/telco-capacity-rules/spec/vectors.json";
  std::vector<Vector> vectors = load_vectors(path);
  check("golden vectors loaded from " + std::string(path), !vectors.empty());

  for (size_t i = 0; i < vectors.size(); i++) {
    const Vector &v = vectors[i];
    check("golden " + v.name + " available_capacity",
          available_capacity(v.total, v.allocated, v.buffer) == v.available);
    check("golden " + v.name + " utilization_pct",
          fabs(utilization_pct(v.total, v.allocated, v.buffer) - v.utilization) < 0.000001);

    Location l;
    l.total_cap_mbps = v.total;
    l.alloc_cap_mbps = v.allocated;
    l.maint_buffer_mbps = v.buffer;
    check("golden " + v.name + " location_available_mbps", location_available_mbps(l) == v.available);
  }

  check("buffer defaults to 0", available_capacity(1000, 600) == 400);
  check("rule names the maintenance buffer",
        strcmp(capacity_rule(), "AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS - MAINT_BUFFER_MBPS") == 0);

  Location riv;
  riv.total_cap_mbps = 1000;
  riv.alloc_cap_mbps = 600;
  riv.maint_buffer_mbps = 150;
  check("RIV-01 sells 250 of 1000", location_available_mbps(riv) == 250);
  check("RIV-01 can support 250", location_can_support(riv, 250));
  check("RIV-01 cannot support 350", !location_can_support(riv, 350));

  printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
