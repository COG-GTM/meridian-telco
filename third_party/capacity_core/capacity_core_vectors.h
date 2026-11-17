#ifndef TELCO_CAPACITY_CORE_VECTORS_H
#define TELCO_CAPACITY_CORE_VECTORS_H

/* Loader for test_vectors.json so C++ consumers can run the shared golden
   cases. Handles exactly the shape of that file: {"cases":[{flat object}...]}
   with string, number and boolean values. Not a general JSON parser. */

#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace capacity_core {

struct Vector {
  std::string name;
  long long total_mbps, allocated_mbps, maintenance_buffer_mbps, requested_mbps;
  long long expect_available_mbps;
  double expect_utilization_pct;
  bool expect_can_support;
};

namespace detail {

inline void skip_ws(const std::string &s, size_t &i) {
  while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++;
}

inline std::string read_string(const std::string &s, size_t &i) {
  std::string out;
  i++; /* opening quote */
  while (i < s.size() && s[i] != '"') {
    if (s[i] == 0x5c && i + 1 < s.size()) i++; /* backslash escape */
    out += s[i++];
  }
  i++; /* closing quote */
  return out;
}

inline std::string read_scalar(const std::string &s, size_t &i) {
  skip_ws(s, i);
  if (s[i] == '"') return read_string(s, i);
  size_t start = i;
  while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ' ' && s[i] != '\n' && s[i] != '\r') i++;
  return s.substr(start, i - start);
}

}  // namespace detail

inline std::vector<Vector> load_vectors(const std::string &path) {
  std::vector<Vector> out;
  std::ifstream f(path.c_str(), std::ios::binary);
  if (!f.good()) return out;
  std::stringstream ss;
  ss << f.rdbuf();
  std::string s = ss.str();
  size_t i = s.find("\"cases\"");
  if (i == std::string::npos) return out;
  i = s.find('[', i);
  while (i != std::string::npos && i < s.size()) {
    size_t open = s.find('{', i);
    size_t end = s.find(']', i);
    if (open == std::string::npos || (end != std::string::npos && end < open)) break;
    i = open + 1;
    std::map<std::string, std::string> kv;
    while (i < s.size()) {
      detail::skip_ws(s, i);
      if (s[i] == '}') { i++; break; }
      if (s[i] == ',') { i++; continue; }
      std::string key = detail::read_string(s, i);
      detail::skip_ws(s, i);
      i++; /* colon */
      kv[key] = detail::read_scalar(s, i);
    }
    Vector v;
    v.name = kv["name"];
    v.total_mbps = atoll(kv["total_mbps"].c_str());
    v.allocated_mbps = atoll(kv["allocated_mbps"].c_str());
    v.maintenance_buffer_mbps = atoll(kv["maintenance_buffer_mbps"].c_str());
    v.requested_mbps = atoll(kv["requested_mbps"].c_str());
    v.expect_available_mbps = atoll(kv["expect_available_mbps"].c_str());
    v.expect_utilization_pct = strtod(kv["expect_utilization_pct"].c_str(), 0);
    v.expect_can_support = kv["expect_can_support"] == "true";
    out.push_back(v);
  }
  return out;
}

}  // namespace capacity_core

#endif
