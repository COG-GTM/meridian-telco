#ifndef MERIDIAN_LATEFEE_H
#define MERIDIAN_LATEFEE_H

/* late fee rules live in telco-rules (third_party/telco-rules) */
#include "telco_rules/latefee.h"

using telco_rules::LATE_FEE_GRACE_DAYS;
using telco_rules::LATE_FEE_PCT;
using telco_rules::days_past_due;
using telco_rules::late_fee;

#endif
