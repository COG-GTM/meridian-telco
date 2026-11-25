package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/lines.cpp: 3-9 lines 5% off recurring, 10+ lines 10%. */
@Component
public class LinesRule {

  private static final BigDecimal HUNDRED = BigDecimal.valueOf(100);

  public BigDecimal multiLinePct(int lineCount) {
    if (lineCount >= 10) return BigDecimal.TEN;
    if (lineCount >= 3) return BigDecimal.valueOf(5);
    return BigDecimal.ZERO;
  }

  public BigDecimal multiLineDiscount(BigDecimal recurringCharge, int lineCount) {
    return Money.round(d(recurringCharge).multiply(d(multiLinePct(lineCount))).divide(d(HUNDRED)));
  }
}
