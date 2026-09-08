#ifndef MERIDIAN_LATEFEE_H
#define MERIDIAN_LATEFEE_H

#include <string>

long days_past_due(const std::string &due_date, const std::string &period);
double late_fee(double prior_balance, const std::string &due_date, const std::string &period);

#endif
