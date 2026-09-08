#ifndef MERIDIAN_RULES_PROFILE_H
#define MERIDIAN_RULES_PROFILE_H

#include "telco_rules/billing.h"

/* Meridian's long-standing billing rationale lives in the shared library:
   whole-GB rating, 30-day proration, issue-cycle promotions, full-month
   suspension billing, and post-loyalty provincial tax. */
inline const telco_rules::Profile& billing_profile() {
  static const telco_rules::Profile profile = telco_rules::meridian_profile();
  return profile;
}

#endif
