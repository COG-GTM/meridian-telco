#ifndef BILLING_RULES_DATECALC_H
#define BILLING_RULES_DATECALC_H

#include <cstdlib>
#include <string>

/* Date helpers for the billing rules. Dates arrive as YYYY-MM-DD strings,
   periods as YYYY-MM. */

namespace billing_rules {

struct BillDate {
  int y;
  int m;
  int d;
  bool ok;
};

inline BillDate parse_date(const std::string &s) {
  BillDate out;
  out.y = 0; out.m = 0; out.d = 0; out.ok = false;
  if (s.size() < 10) return out;
  out.y = atoi(s.substr(0, 4).c_str());
  out.m = atoi(s.substr(5, 2).c_str());
  out.d = atoi(s.substr(8, 2).c_str());
  out.ok = out.y > 0 && out.m > 0 && out.d > 0;
  return out;
}

inline BillDate period_start(const std::string &period) {
  BillDate out;
  out.y = 0; out.m = 0; out.d = 1; out.ok = false;
  if (period.size() < 7) return out;
  out.y = atoi(period.substr(0, 4).c_str());
  out.m = atoi(period.substr(5, 2).c_str());
  out.ok = out.y > 0 && out.m > 0;
  return out;
}

/* days since 1970-01-01, howard hinnant's civil_from_days in reverse. */
inline long serial_day(const BillDate &dt) {
  int y = dt.y;
  y -= dt.m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (dt.m + (dt.m > 2 ? -3 : 9)) + 2) / 5 + dt.d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

inline long days_between(const BillDate &from, const BillDate &to) {
  return serial_day(to) - serial_day(from);
}

inline int calendar_days_in_month(int y, int m) {
  static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 30;
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  return len[m - 1];
}

}  // namespace billing_rules

#endif
