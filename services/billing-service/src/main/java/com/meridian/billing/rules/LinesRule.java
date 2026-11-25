package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/lines.cpp: 3-9 lines 5% off recurring, 10+ lines 10%. */
@Component
public class LinesRule {

  public BigDecimal multiLinePct(int lineCount) {
    throw new UnsupportedOperationException("Phase 3");
  }

  public BigDecimal multiLineDiscount(BigDecimal recurringCharge, int lineCount) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
