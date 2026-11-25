package com.meridian.billing.calc;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.billing.model.Account;
import com.meridian.billing.model.Invoice;
import com.meridian.billing.rules.AccountsRule;
import com.meridian.billing.rules.InvoiceComposer;
import com.meridian.billing.rules.TaxRule;
import com.meridian.contracts.testkit.MeridianRoot;
import java.math.BigDecimal;
import java.math.RoundingMode;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;
import org.junit.jupiter.api.Test;

/**
 * billing/money.h rounds a C++ double, so a product like 1102.50 * 0.13 = 143.325 (exactly, in
 * decimal) is 143.32499999999998863... in binary64 and rounds down. The frozen register has the
 * double result, so that is what billing-service reproduces.
 */
class LegacyRoundingTest {

  @ParameterizedTest(name = "money({0}) = {1}")
  @CsvSource({
      "0.125, 0.13",     // exact in binary64
      "1.005, 1.00",     // 1.00499999999999989...
      "2.675, 2.68",     // 2.67499999999999982... * 100 + 0.5 rounds up to 268.0 in binary64
      "143.325, 143.32",
      "-0.005, -0.00",
      "19.999999, 20.00"})
  void roundsLikeTheLegacyDouble(String in, String want) {
    assertThat(Money.round(new BigDecimal(in))).isEqualByComparingTo(want);
  }

  @Test
  void binary64HoldsTheNearestDouble() {
    assertThat(Binary64.of("0.1").exact())
        .isEqualByComparingTo("0.1000000000000000055511151231257827021181583404541015625");
    assertThat(Binary64.of("0.1").add(Binary64.of("0.2")).exact())
        .isEqualByComparingTo("0.3000000000000000444089209850062616169452667236328125");
  }

  @Test
  void onHstOnMer012267IsTheLegacyValueNotTheExactDecimalOne() {
    TaxRule tax = new TaxRule();
    BigDecimal subtotal = new BigDecimal("1102.50");
    assertThat(subtotal.multiply(new BigDecimal("0.13")).setScale(2, RoundingMode.HALF_UP))
        .as("exact decimal").isEqualByComparingTo("143.33");
    assertThat(tax.federalTax(subtotal, tax.ratesFor("ON"))).as("legacy double").isEqualByComparingTo("143.32");
  }

  @ParameterizedTest
  @CsvSource({"2026-06, 1024577", "2026-07, 1024646"})
  void mer012267InvoiceMatchesTheFrozenRegister(String period, long usageMb) {
    Account a = AccountsRule.index(AccountsRule.loadAccounts(MeridianRoot.resolve("billing/accounts")))
        .get("MER-012267");
    Invoice inv = InvoiceComposer.standard().compose(a, new com.meridian.billing.model.UsageRecord(
        "MER-012267", period, usageMb));
    assertThat(inv.subtotal()).isEqualByComparingTo("1102.50");
    assertThat(inv.federalTax()).isEqualByComparingTo("143.32");
    assertThat(inv.total()).isEqualByComparingTo("1201.72");
  }
}
