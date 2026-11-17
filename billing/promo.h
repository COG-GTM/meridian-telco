#ifndef MERIDIAN_PROMO_H
#define MERIDIAN_PROMO_H

#include <string_view>

bool promo_is_live(std::string_view issued_on, std::string_view period);
double promo_credit(double amount, std::string_view issued_on, std::string_view period);

#endif
