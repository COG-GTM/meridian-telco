#ifndef BILLING_RULES_CONFIG_H
#define BILLING_RULES_CONFIG_H

/* generated from rules.json by tools/generate.py -- do not edit by hand */

#define BR_SPEC_VERSION 1
#define BR_DEFAULT_PROFILE "meridian"

#define BR_LINE_ROUNDING_PER_LINE 1

#define BR_BILLING_MONTH_DAYS 30
#define BR_PRORATION_CALENDAR_MONTH 0

#define BR_MB_PER_GB 1024
#define BR_OVERAGE_RATE_PER_GB 10.000000
#define BR_OVERAGE_RATE_PER_MB 0.012000
#define BR_RATING_EXACT_MB 0

#define BR_PROMO_VALID_DAYS 30
#define BR_PROMO_EXPIRY_ROLLING_DAYS 0

#define BR_SUSPENSION_CREDIT_DAILY_RATE 0

#define BR_LATE_FEE_GRACE_DAYS 10
#define BR_LATE_FEE_PCT 1.500000

#define BR_PROVINCIAL_TAX_POST_DISCOUNT 1

#define BR_LINE_TIER_COUNT 2
#define BR_PROVINCE_COUNT 4

namespace billing_rules {

static const int BR_LINE_TIER_MIN[BR_LINE_TIER_COUNT] = {10, 3};
static const double BR_LINE_TIER_PCT[BR_LINE_TIER_COUNT] = {10.000000, 5.000000};

static const char *const BR_PROVINCE_CODE[BR_PROVINCE_COUNT] = {"BC", "AB", "ON", "QC"};
static const double BR_PROVINCE_FEDERAL_PCT[BR_PROVINCE_COUNT] = {5.000000, 5.000000, 13.000000, 5.000000};
static const double BR_PROVINCE_PROVINCIAL_PCT[BR_PROVINCE_COUNT] = {7.000000, 0.000000, 0.000000, 9.975000};
static const char *const BR_PROVINCE_FEDERAL_LABEL[BR_PROVINCE_COUNT] = {"GST", "GST", "HST", "GST"};
static const char *const BR_PROVINCE_PROVINCIAL_LABEL[BR_PROVINCE_COUNT] = {"PST", "", "", "QST"};

static const double BR_DEFAULT_FEDERAL_PCT = 5.000000;
static const double BR_DEFAULT_PROVINCIAL_PCT = 0.000000;
static const char *const BR_DEFAULT_FEDERAL_LABEL = "GST";
static const char *const BR_DEFAULT_PROVINCIAL_LABEL = "";

}  // namespace billing_rules

#endif
