package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.Money;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/**
 * billing/discounts.cpp: the loyalty credit comes off the subtotal, before the provincial
 * component of the tax is worked out (see {@link TaxRule}).
 */
@Component
public class DiscountsRule {

  private static final BigDecimal HUNDRED = BigDecimal.valueOf(100);

  public BigDecimal loyaltyDiscount(BigDecimal amount, BigDecimal loyaltyPct) {
    return Money.round(d(amount).multiply(d(loyaltyPct)).divide(d(HUNDRED)));
  }

  public BigDecimal applyLoyalty(BigDecimal amount, BigDecimal loyaltyPct) {
    return Money.round(d(amount).subtract(d(loyaltyDiscount(amount, loyaltyPct))));
  }
}
