#include "rating.h"

long usage_gb_rounded(long usage_mb) {
  return telco_rules::usage_gb_rounded(usage_mb);
}

long overage_gb(long usage_mb, long included_gb) {
  return telco_rules::overage_gb(usage_mb, included_gb);
}

double rate_overage(long usage_mb, long included_gb) {
  return telco_rules::rate_overage(usage_mb, included_gb);
}
