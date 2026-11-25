package com.meridian.inventory.api;

import java.math.BigDecimal;
import java.math.MathContext;
import java.math.RoundingMode;

/** Text rendering of the legacy inventory-api JSON (snprintf over SQLite text values). */
final class LegacyJson {

  private static final MathContext SQLITE_REAL = new MathContext(15, RoundingMode.HALF_EVEN);

  private LegacyJson() {}

  /** httpd.h json_escape: only '"' and '\' are escaped. */
  static String escape(String s) {
    if (s == null) return "";
    StringBuilder o = new StringBuilder(s.length() + 8);
    for (int i = 0; i < s.length(); i++) {
      char c = s.charAt(i);
      if (c == '"' || c == '\\') o.append('\\');
      o.append(c);
    }
    return o.toString();
  }

  static String raw(String s) {
    return s == null ? "" : s;
  }

  /**
   * How SQLite renders a REAL column as text ("%!.15g"): 15 significant digits, trailing zeros
   * dropped but at least one fractional digit, exponent form below 1e-4 or from 1e15.
   */
  static String sqliteReal(double d) {
    if (Double.isNaN(d)) return "NaN";
    if (Double.isInfinite(d)) return d > 0 ? "Inf" : "-Inf";
    if (d == 0) return "0.0";
    BigDecimal v = new BigDecimal(d).round(SQLITE_REAL);
    int exp = v.precision() - v.scale() - 1;
    if (exp < -4 || exp >= 15) {
      String mantissa = withFraction(v.movePointLeft(exp).stripTrailingZeros().toPlainString());
      return mantissa + "e" + (exp < 0 ? "-" : "+") + (Math.abs(exp) < 10 ? "0" : "") + Math.abs(exp);
    }
    return withFraction(v.stripTrailingZeros().toPlainString());
  }

  private static String withFraction(String s) {
    return s.indexOf('.') >= 0 ? s : s + ".0";
  }
}
