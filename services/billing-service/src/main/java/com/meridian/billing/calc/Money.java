package com.meridian.billing.calc;

import java.math.BigDecimal;
import java.math.BigInteger;

/**
 * billing/money.h: every charge line is rounded to cents as it is produced, with
 * {@code floor(amount * 100.0 + 0.5) / 100.0} on a double.
 *
 * <p>Amounts leave this class as BigDecimal cents (scale 2). Because the legacy result
 * {@code k / 100.0} is the double nearest to {@code k/100}, {@link Binary64#of(BigDecimal)} on a
 * cents value gives back exactly the double the C++ code carried into the next step.
 */
public final class Money {

  public static final BigDecimal ZERO = new BigDecimal("0.00");

  private static final Binary64 HUNDRED = Binary64.of(100);
  private static final Binary64 HALF = Binary64.of("0.5");

  private Money() {}

  /** Legacy {@code money(x)}. */
  public static BigDecimal round(Binary64 amount) {
    BigInteger cents = amount.multiply(HUNDRED).add(HALF).floor().exact().toBigIntegerExact();
    return new BigDecimal(cents, 2);
  }

  /** Legacy {@code money(x)} on an amount that was already a double (an atof'd field). */
  public static BigDecimal round(BigDecimal amount) {
    return round(Binary64.of(amount));
  }

  /** Shorthand for lifting a BigDecimal into the legacy double arithmetic. */
  public static Binary64 d(BigDecimal amount) {
    return Binary64.of(amount);
  }
}
