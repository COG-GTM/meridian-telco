package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/suspension.cpp: a suspended line is billed the full month, no credit. */
@Component
public class SuspensionRule {

  public int suspendedDays(int startDay, int endDay) {
    throw new UnsupportedOperationException("Phase 3");
  }

  public BigDecimal credit(BigDecimal monthlyFee, int startDay, int endDay) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
