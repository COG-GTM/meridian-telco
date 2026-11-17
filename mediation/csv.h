#ifndef MERIDIAN_CSV_H
#define MERIDIAN_CSV_H

#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using Row = std::map<std::string, std::string>;

inline std::vector<std::string> split_line(std::string_view line) {
  std::vector<std::string> out;
  std::string cur;
  bool quoted = false;
  for (char c : line) {
    if (c == '"') { quoted = !quoted; continue; }
    if (c == ',' && !quoted) { out.push_back(std::move(cur)); cur.clear(); continue; }
    cur += c;
  }
  out.push_back(std::move(cur));
  return out;
}

inline std::vector<Row> read_csv(const std::string &path) {
  std::vector<Row> rows;
  std::ifstream f(path);
  if (!f.good()) return rows;
  std::string line;
  if (!std::getline(f, line)) return rows;
  const std::vector<std::string> hdr = split_line(line);
  while (std::getline(f, line)) {
    if (line.empty()) continue;
    const std::vector<std::string> v = split_line(line);
    Row r;
    for (size_t i = 0; i < hdr.size() && i < v.size(); i++) r[hdr[i]] = v[i];
    rows.push_back(std::move(r));
  }
  return rows;
}

/* read-only column lookup; unlike Row::operator[] it never inserts */
inline std::optional<std::string_view> find_field(const Row &r, std::string_view key) {
  if (auto it = r.find(std::string(key)); it != r.end()) return it->second;
  return std::nullopt;
}

inline std::string_view field(const Row &r, std::string_view key) {
  return find_field(r, key).value_or(std::string_view{});
}

/* atoi/atof semantics (leading whitespace, sign, stop at first non-digit) */
inline int to_int(std::string_view s) {
  if (s.empty()) return 0;
  return std::atoi(std::string(s).c_str());
}

inline double to_dbl(std::string_view s) {
  if (s.empty()) return 0.0;
  return std::atof(std::string(s).c_str());
}

#endif
