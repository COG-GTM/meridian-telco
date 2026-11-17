#ifndef MERIDIAN_STATUS_H
#define MERIDIAN_STATUS_H

#include <string>

inline constexpr int STATUS_ACTIVE = 1;
inline constexpr int STATUS_PLANNED = 2;
inline constexpr int STATUS_DECOMM = 3;

std::string status_label(int status_cd);
bool status_is_countable(int status_cd);

#endif
