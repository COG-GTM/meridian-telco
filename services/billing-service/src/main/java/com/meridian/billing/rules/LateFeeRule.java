package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.BillDate;
import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/latefee.cpp: 10 day grace after the due date, then 1.5% of the balance. */
@Component
public class LateFeeRule {

  public static final int GRACE_DAYS = 10;
  public static final BigDecimal LATE_FEE_PCT = new BigDecimal("1.5");

  private static final BigDecimal HUNDRED = BigDecimal.valueOf(100);

  public long daysPastDue(String dueDate, String period) {
    BillDate due = BillDate.parseDate(dueDate);
    BillDate cycle = BillDate.periodStart(period);
    if (!due.ok() || !cycle.ok()) return 0;
    return Math.max(0, BillDate.daysBetween(due, cycle));
  }

  public BigDecimal lateFee(BigDecimal priorBalance, String dueDate, String period) {
    if (priorBalance.signum() <= 0) return Money.ZERO;
    if (daysPastDue(dueDate, period) <= GRACE_DAYS) return Money.ZERO;
    return Money.round(d(priorBalance).multiply(d(LATE_FEE_PCT)).divide(d(HUNDRED)));
  }
}
