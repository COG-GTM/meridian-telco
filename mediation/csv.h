#ifndef MERIDIAN_CSV_H
#define MERIDIAN_CSV_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdlib>

typedef std::map<std::string, std::string> Row;

inline std::vector<std::string> split_line(const std::string &line) {
  std::vector<std::string> out;
  std::string cur;
  bool q = false;
  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];
    if (c == '"') { q = !q; continue; }
    if (c == ',' && !q) { out.push_back(cur); cur.clear(); continue; }
    cur += c;
  }
  out.push_back(cur);
  return out;
}

inline std::vector<Row> read_csv(const std::string &path) {
  std::vector<Row> rows;
  std::ifstream f(path.c_str());
  if (!f.good()) return rows;
  std::string line;
  if (!std::getline(f, line)) return rows;
  if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
  std::vector<std::string> hdr = split_line(line);
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    if (line[line.size() - 1] == '\r') line.erase(line.size() - 1);
    std::vector<std::string> v = split_line(line);
    Row r;
    for (size_t i = 0; i < hdr.size() && i < v.size(); i++) r[hdr[i]] = v[i];
    rows.push_back(r);
  }
  return rows;
}

inline int to_int(const std::string &s) {
  if (s.empty()) return 0;
  return atoi(s.c_str());
}

inline double to_dbl(const std::string &s) {
  if (s.empty()) return 0.0;
  return atof(s.c_str());
}

#endif
