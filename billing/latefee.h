#ifndef MERIDIAN_LATEFEE_H
#define MERIDIAN_LATEFEE_H

#include <string_view>

#define LATE_FEE_GRACE_DAYS 10
#define LATE_FEE_PCT 1.5

long days_past_due(std::string_view due_date, std::string_view period);
double late_fee(double prior_balance, std::string_view due_date, std::string_view period);

#endif
