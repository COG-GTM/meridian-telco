package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/tax.cpp: GST/HST on the pre-discount amount, PST/QST on what is paid. */
@Component
public class TaxRule {

  public TaxRates ratesFor(String province) {
    throw new UnsupportedOperationException("Phase 3");
  }

  public BigDecimal federalTax(BigDecimal preDiscountAmount, TaxRates rates) {
    throw new UnsupportedOperationException("Phase 3");
  }

  public BigDecimal provincialTax(BigDecimal preDiscountAmount, BigDecimal discount, TaxRates rates) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
