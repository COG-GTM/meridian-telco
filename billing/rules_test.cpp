#include <gtest/gtest.h>
#include "latefee.h"
#include "lines.h"
#include "promo.h"
#include "proration.h"
#include "suspension.h"
#include "tax.h"

TEST(BillingRules, FullMonthIsThePlanFee) {
  EXPECT_NEAR(prorated_plan_charge(640.0, 0.0, 0), 640.0, 0.005);
}

TEST(BillingRules, HalfMonthOnEachPlan) {
  EXPECT_NEAR(prorated_plan_charge(300.0, 600.0, 16), 450.0, 0.005);
}

TEST(BillingRules, JulyAndFebruaryProrateAlike) {
  EXPECT_NEAR(prorated_plan_charge(300.0, 600.0, 16), prorated_plan_charge(300.0, 600.0, 16), 0.005);
}

TEST(BillingRules, PromoIssuedInCycleIsLive) {
  EXPECT_TRUE(promo_is_live("2026-07-04", "2026-07"));
}

TEST(BillingRules, PromoIssuedLastCycleIsDead) {
  EXPECT_TRUE(!promo_is_live("2026-06-24", "2026-07"));
}

TEST(BillingRules, DeadPromoCreditsNothing) {
  EXPECT_NEAR(promo_credit(120.0, "2026-06-24", "2026-07"), 0.0, 0.005);
}

TEST(BillingRules, SuspendedDaysCounted) {
  EXPECT_NEAR((double)suspended_days(10, 14), 5.0, 0.005);
}

TEST(BillingRules, SuspensionCreditsNothing) {
  EXPECT_NEAR(suspension_credit(620.0, 10, 14), 0.0, 0.005);
}

TEST(BillingRules, TwoLinesNoDiscount) {
  EXPECT_NEAR(multi_line_pct(2), 0.0, 0.005);
}

TEST(BillingRules, ThreeLinesFivePercent) {
  EXPECT_NEAR(multi_line_pct(3), 5.0, 0.005);
}

TEST(BillingRules, TenLinesTenPercent) {
  EXPECT_NEAR(multi_line_pct(10), 10.0, 0.005);
}

TEST(BillingRules, DiscountOffRecurring) {
  EXPECT_NEAR(multi_line_discount(1000.0, 12), 100.0, 0.005);
}

TEST(BillingRules, InsideGraceNoFee) {
  EXPECT_NEAR(late_fee(500.0, "2026-06-30", "2026-07"), 0.0, 0.005);
}

TEST(BillingRules, PastGraceOneAndAHalfPercent) {
  EXPECT_NEAR(late_fee(500.0, "2026-06-01", "2026-07"), 7.5, 0.005);
}

TEST(BillingRules, BcGstOnPreDiscount) {
  const auto bc = rates_for_province("BC");
  EXPECT_NEAR(federal_tax(100.0, bc), 5.0, 0.005);
}

TEST(BillingRules, BcPstOnPostDiscount) {
  const auto bc = rates_for_province("BC");
  EXPECT_NEAR(provincial_tax(100.0, 10.0, bc), 6.30, 0.005);
}

TEST(BillingRules, OntarioHstIs13) {
  const auto on = rates_for_province("ON");
  EXPECT_NEAR(federal_tax(100.0, on), 13.0, 0.005);
}

TEST(BillingRules, OntarioHasNoSeparateProvincialLine) {
  const auto on = rates_for_province("ON");
  EXPECT_NEAR(provincial_tax(100.0, 10.0, on), 0.0, 0.005);
}

TEST(BillingRules, QuebecProvincialLineIsQst) {
  const auto qc = rates_for_province("QC");
  EXPECT_TRUE(qc.provincial_label == "QST");
}

TEST(BillingRules, QuebecQstOnPostDiscount) {
  const auto qc = rates_for_province("QC");
  EXPECT_NEAR(provincial_tax(1000.0, 100.0, qc), 89.78, 0.005);
}

TEST(BillingRules, AlbertaHasNoProvincialTax) {
  const auto ab = rates_for_province("AB");
  EXPECT_NEAR(provincial_tax(100.0, 0.0, ab), 0.0, 0.005);
}
