package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.Binary64;
import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/**
 * billing/proration.cpp: the billing month is 30 days, always, whatever the calendar says. A
 * mid-cycle plan change bills {@code changeDay - 1} days on the old fee and the rest on the new.
 */
@Component
public class ProrationRule {

  public static final int BILLING_MONTH_DAYS = 30;

  public Binary64 dailyRate(BigDecimal monthlyFee) {
    return d(monthlyFee).divide(BILLING_MONTH_DAYS);
  }

  public BigDecimal proratedPlanCharge(BigDecimal monthlyFee, BigDecimal prevMonthlyFee, int changeDay) {
    if (changeDay <= 0) return Money.round(monthlyFee);
    int daysOnOld = Math.clamp(changeDay - 1, 0, BILLING_MONTH_DAYS);
    int daysOnNew = BILLING_MONTH_DAYS - daysOnOld;
    BigDecimal oldPart = Money.round(dailyRate(prevMonthlyFee).multiply(daysOnOld));
    BigDecimal newPart = Money.round(dailyRate(monthlyFee).multiply(daysOnNew));
    return Money.round(d(oldPart).add(d(newPart)));
  }
}
