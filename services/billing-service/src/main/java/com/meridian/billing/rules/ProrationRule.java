package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/proration.cpp: 30 day billing month, mid-cycle plan changes split on it. */
@Component
public class ProrationRule {

  public BigDecimal proratedPlanCharge(BigDecimal monthlyFee, BigDecimal prevMonthlyFee, int changeDay) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
