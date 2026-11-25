package com.meridian.billing.rules;

import com.meridian.billing.calc.BillDate;
import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/promo.cpp: promo credits expire at the end of the cycle they were issued in. */
@Component
public class PromoRule {

  public boolean isLive(String issuedOn, String period) {
    BillDate issued = BillDate.parseDate(issuedOn);
    BillDate cycle = BillDate.periodStart(period);
    if (!issued.ok() || !cycle.ok()) return false;
    return issued.y() == cycle.y() && issued.m() == cycle.m();
  }

  public BigDecimal credit(BigDecimal amount, String issuedOn, String period) {
    if (amount.signum() <= 0) return Money.ZERO;
    if (!isLive(issuedOn, period)) return Money.ZERO;
    return Money.round(amount);
  }
}
