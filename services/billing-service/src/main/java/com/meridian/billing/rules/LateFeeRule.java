package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/latefee.cpp: 10 day grace after the due date, then 1.5% of the balance. */
@Component
public class LateFeeRule {

  public BigDecimal lateFee(BigDecimal priorBalance, String dueDate, String period) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
