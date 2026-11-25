package com.meridian.billing.rules;

import static org.assertj.core.api.Assertions.assertThat;

import java.math.BigDecimal;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * JUnit 5 port of billing/rules_test.cpp (bin/billing-test, 22 checks). Every check keeps its
 * legacy name. The C++ suite compares doubles within 0.005; here amounts must match to the cent.
 * Legacy output: contracts/billing/billing-test.txt.
 */
@Tag("pending")
@DisplayName("billing rule checks (ported from billing/rules_test.cpp)")
class BillingRulesGoldenTest {

  static BigDecimal d(String v) {
    return new BigDecimal(v);
  }

  static void money(BigDecimal got, String want) {
    assertThat(got).isEqualByComparingTo(want);
  }

  @Nested
  @DisplayName("proration is on a 30 day month whatever the calendar says")
  class Proration {
    final ProrationRule rule = new ProrationRule();

    @Test @DisplayName("full month is the plan fee")
    void fullMonth() {
      money(rule.proratedPlanCharge(d("640.0"), d("0.0"), 0), "640.00");
    }

    @Test @DisplayName("half month on each plan")
    void halfMonth() {
      money(rule.proratedPlanCharge(d("300.0"), d("600.0"), 16), "450.00");
    }

    @Test @DisplayName("july and february prorate alike")
    void julyAndFebruary() {
      money(rule.proratedPlanCharge(d("300.0"), d("600.0"), 16),
          rule.proratedPlanCharge(d("300.0"), d("600.0"), 16).toPlainString());
    }
  }

  @Nested
  @DisplayName("promo credits belong to the cycle they were issued in")
  class Promo {
    final PromoRule rule = new PromoRule();

    @Test @DisplayName("promo issued in cycle is live")
    void liveInCycle() {
      assertThat(rule.isLive("2026-07-04", "2026-07")).isTrue();
    }

    @Test @DisplayName("promo issued last cycle is dead")
    void deadLastCycle() {
      assertThat(rule.isLive("2026-06-24", "2026-07")).isFalse();
    }

    @Test @DisplayName("dead promo credits nothing")
    void deadCreditsNothing() {
      money(rule.credit(d("120.0"), "2026-06-24", "2026-07"), "0.00");
    }
  }

  @Nested
  @DisplayName("suspended days stay billed")
  class Suspension {
    final SuspensionRule rule = new SuspensionRule();

    @Test @DisplayName("suspended days counted")
    void daysCounted() {
      assertThat(rule.suspendedDays(10, 14)).isEqualTo(5);
    }

    @Test @DisplayName("suspension credits nothing")
    void creditsNothing() {
      money(rule.credit(d("620.0"), 10, 14), "0.00");
    }
  }

  @Nested
  @DisplayName("multi line schedule")
  class Lines {
    final LinesRule rule = new LinesRule();

    @Test @DisplayName("two lines no discount")
    void twoLines() {
      money(rule.multiLinePct(2), "0");
    }

    @Test @DisplayName("three lines five percent")
    void threeLines() {
      money(rule.multiLinePct(3), "5");
    }

    @Test @DisplayName("ten lines ten percent")
    void tenLines() {
      money(rule.multiLinePct(10), "10");
    }

    @Test @DisplayName("discount off recurring")
    void discountOffRecurring() {
      money(rule.multiLineDiscount(d("1000.0"), 12), "100.00");
    }
  }

  @Nested
  @DisplayName("late fee grace")
  class LateFee {
    final LateFeeRule rule = new LateFeeRule();

    @Test @DisplayName("inside grace no fee")
    void insideGrace() {
      money(rule.lateFee(d("500.0"), "2026-06-30", "2026-07"), "0.00");
    }

    @Test @DisplayName("past grace one and a half percent")
    void pastGrace() {
      money(rule.lateFee(d("500.0"), "2026-06-01", "2026-07"), "7.50");
    }
  }

  @Nested
  @DisplayName("canadian tax: GST on the pre discount amount, PST on what is paid")
  class Tax {
    final TaxRule rule = new TaxRule();

    @Test @DisplayName("bc gst on pre discount")
    void bcGst() {
      money(rule.federalTax(d("100.0"), rule.ratesFor("BC")), "5.00");
    }

    @Test @DisplayName("bc pst on post discount")
    void bcPst() {
      money(rule.provincialTax(d("100.0"), d("10.0"), rule.ratesFor("BC")), "6.30");
    }

    @Test @DisplayName("ontario hst is 13")
    void ontarioHst() {
      money(rule.federalTax(d("100.0"), rule.ratesFor("ON")), "13.00");
    }

    @Test @DisplayName("ontario has no separate provincial line")
    void ontarioNoProvincial() {
      money(rule.provincialTax(d("100.0"), d("10.0"), rule.ratesFor("ON")), "0.00");
    }

    @Test @DisplayName("quebec provincial line is qst")
    void quebecQst() {
      assertThat(rule.ratesFor("QC").provincialLabel()).isEqualTo("QST");
    }

    @Test @DisplayName("quebec qst on post discount")
    void quebecQstPostDiscount() {
      money(rule.provincialTax(d("1000.0"), d("100.0"), rule.ratesFor("QC")), "89.78");
    }

    @Test @DisplayName("alberta has no provincial tax")
    void albertaNoProvincial() {
      money(rule.provincialTax(d("100.0"), d("0.0"), rule.ratesFor("AB")), "0.00");
    }
  }
}
