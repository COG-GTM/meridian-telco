package com.meridian.billing.calc;

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.RoundingMode;

/**
 * Exact model of an IEEE 754 binary64 value, held as the BigDecimal it denotes.
 *
 * <p>The legacy C++ billing computed every charge in {@code double} and rounded with
 * {@code floor(x*100+0.5)/100} (billing/money.h). Exact decimal arithmetic disagrees with that at
 * a handful of half-cent ties (e.g. HST on 1102.50 is 143.325: legacy 143.32, HALF_UP 143.33), so
 * to keep the register byte-identical each legacy operation is reproduced here: the exact result
 * is computed with BigInteger/BigDecimal and rounded to the nearest binary64 (ties to even), the
 * same thing the FPU did. No {@code double} is used anywhere. Subnormals and overflow are not
 * modelled; billing amounts are many orders of magnitude away from either.
 */
public final class Binary64 implements Comparable<Binary64> {

  private static final int PRECISION = 53;
  private static final BigInteger FIVE = BigInteger.valueOf(5);

  public static final Binary64 ZERO = new Binary64(BigDecimal.ZERO);

  private final BigDecimal exact;

  private Binary64(BigDecimal exact) {
    this.exact = exact;
  }

  /** The double nearest to {@code value}, what C {@code atof} or a literal would produce. */
  public static Binary64 of(BigDecimal value) {
    return new Binary64(nearest(value));
  }

  public static Binary64 of(long value) {
    return of(BigDecimal.valueOf(value));
  }

  public static Binary64 of(String literal) {
    return of(new BigDecimal(literal));
  }

  public Binary64 add(Binary64 o) {
    return of(exact.add(o.exact));
  }

  public Binary64 subtract(Binary64 o) {
    return of(exact.subtract(o.exact));
  }

  public Binary64 multiply(Binary64 o) {
    return of(exact.multiply(o.exact));
  }

  public Binary64 multiply(long o) {
    return multiply(of(o));
  }

  public Binary64 divide(Binary64 o) {
    BigInteger[] a = rational(exact);
    BigInteger[] b = rational(o.exact);
    return new Binary64(nearest(a[0].multiply(b[1]), a[1].multiply(b[0])));
  }

  public Binary64 divide(long o) {
    return divide(of(o));
  }

  /** C {@code floor}; the floor of a binary64 is itself representable. */
  public Binary64 floor() {
    return new Binary64(exact.setScale(0, RoundingMode.FLOOR));
  }

  public int signum() {
    return exact.signum();
  }

  /** The exact value this double denotes. */
  public BigDecimal exact() {
    return exact;
  }

  /** printf {@code %.Nf}: the exact binary value correctly rounded to N places. */
  public BigDecimal printf(int places) {
    return exact.setScale(places, RoundingMode.HALF_EVEN);
  }

  @Override
  public int compareTo(Binary64 o) {
    return exact.compareTo(o.exact);
  }

  @Override
  public boolean equals(Object o) {
    return o instanceof Binary64 b && exact.compareTo(b.exact) == 0;
  }

  @Override
  public int hashCode() {
    return exact.stripTrailingZeros().hashCode();
  }

  @Override
  public String toString() {
    return exact.toPlainString();
  }

  static BigDecimal nearest(BigDecimal v) {
    BigInteger[] r = rational(v);
    return nearest(r[0], r[1]);
  }

  /** num/den as a reduced-free fraction with a positive denominator. */
  private static BigInteger[] rational(BigDecimal v) {
    BigInteger unscaled = v.unscaledValue();
    int scale = v.scale();
    if (scale >= 0) {
      return new BigInteger[] {unscaled, BigInteger.TEN.pow(scale)};
    }
    return new BigInteger[] {unscaled.multiply(BigInteger.TEN.pow(-scale)), BigInteger.ONE};
  }

  /** Round num/den to the nearest binary64, ties to even. */
  private static BigDecimal nearest(BigInteger num, BigInteger den) {
    if (den.signum() == 0) {
      throw new ArithmeticException("division by zero");
    }
    if (num.signum() == 0) {
      return BigDecimal.ZERO;
    }
    boolean negative = num.signum() * den.signum() < 0;
    num = num.abs();
    den = den.abs();
    // find e with 2^52 <= num / (den * 2^e) < 2^53
    int e = num.bitLength() - den.bitLength() - PRECISION;
    BigInteger q;
    BigInteger rem;
    BigInteger d;
    while (true) {
      BigInteger n = e < 0 ? num.shiftLeft(-e) : num;
      d = e > 0 ? den.shiftLeft(e) : den;
      BigInteger[] qr = n.divideAndRemainder(d);
      q = qr[0];
      rem = qr[1];
      if (q.bitLength() > PRECISION) {
        e++;
      } else if (q.bitLength() < PRECISION) {
        e--;
      } else {
        break;
      }
    }
    int half = rem.shiftLeft(1).compareTo(d);
    if (half > 0 || (half == 0 && q.testBit(0))) {
      q = q.add(BigInteger.ONE);
    }
    BigDecimal out = e >= 0
        ? new BigDecimal(q.shiftLeft(e))
        : new BigDecimal(q.multiply(FIVE.pow(-e)), -e);
    out = out.stripTrailingZeros();
    if (out.scale() < 0) {
      out = out.setScale(0);
    }
    return negative ? out.negate() : out;
  }
}
