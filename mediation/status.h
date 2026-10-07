#ifndef MERIDIAN_STATUS_H
#define MERIDIAN_STATUS_H

/* status codes live in telco-rules (third_party/telco-rules), DECISIONS.md D-09 */
#include "telco_rules/status.h"

using telco_rules::STATUS_ACTIVE;
using telco_rules::STATUS_PLANNED;
using telco_rules::STATUS_DECOMM;
using telco_rules::status_label;
using telco_rules::status_is_countable;

#endif
