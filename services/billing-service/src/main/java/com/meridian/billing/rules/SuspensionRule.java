package com.meridian.billing.rules;

import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/suspension.cpp: a suspended line is billed the full month, no credit. */
@Component
public class SuspensionRule {

  public int suspendedDays(int startDay, int endDay) {
    if (startDay <= 0 || endDay < startDay) return 0;
    return endDay - startDay + 1;
  }

  /** A suspended line keeps its number and provisioning, so there is never a partial credit. */
  public BigDecimal credit(BigDecimal monthlyFee, int startDay, int endDay) {
    return Money.ZERO;
  }
}
