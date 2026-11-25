package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.Binary64;
import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/**
 * billing/tax.cpp: GST (or HST in a harmonized province) is assessed on the charge before the
 * loyalty discount; PST/QST on what the customer actually pays, so the discount comes off first.
 */
@Component
public class TaxRule {

  private static final BigDecimal HUNDRED = BigDecimal.valueOf(100);

  private static final TaxRates BC = rates("5.0", "7.0", "GST", "PST");
  private static final TaxRates AB = rates("5.0", "0.0", "GST", "");
  private static final TaxRates ON = rates("13.0", "0.0", "HST", "");
  private static final TaxRates QC = rates("5.0", "9.975", "GST", "QST");
  private static final TaxRates OTHER = rates("5.0", "0.0", "GST", "");

  private static TaxRates rates(String fed, String prov, String fedLabel, String provLabel) {
    return new TaxRates(new BigDecimal(fed), new BigDecimal(prov), fedLabel, provLabel);
  }

  public TaxRates ratesFor(String province) {
    return switch (province == null ? "" : province) {
      case "BC" -> BC;
      case "AB" -> AB;
      case "ON" -> ON;
      case "QC" -> QC;
      default -> OTHER;
    };
  }

  public BigDecimal federalTax(BigDecimal preDiscountAmount, TaxRates rates) {
    return Money.round(d(preDiscountAmount).multiply(d(rates.federalPct())).divide(d(HUNDRED)));
  }

  public BigDecimal provincialTax(BigDecimal preDiscountAmount, BigDecimal discount, TaxRates rates) {
    if (rates.provincialPct().signum() <= 0) return Money.ZERO;
    Binary64 base = d(preDiscountAmount).subtract(d(discount));
    if (base.signum() < 0) base = Binary64.ZERO;
    return Money.round(base.multiply(d(rates.provincialPct())).divide(d(HUNDRED)));
  }
}
