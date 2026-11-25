package com.meridian.billing.calc;

import java.math.BigDecimal;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** C {@code atoi}/{@code atol}/{@code atof} on the flat files: longest numeric prefix, else 0. */
public final class CNum {

  private static final Pattern DECIMAL =
      Pattern.compile("^[ \\t\\n\\x0B\\f\\r]*([+-]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][+-]?\\d+)?)");
  private static final Pattern INTEGER = Pattern.compile("^[ \\t\\n\\x0B\\f\\r]*([+-]?\\d+)");

  private CNum() {}

  public static BigDecimal atof(String s) {
    if (s == null) return BigDecimal.ZERO;
    Matcher m = DECIMAL.matcher(s);
    return m.find() ? new BigDecimal(m.group(1)) : BigDecimal.ZERO;
  }

  public static long atol(String s) {
    if (s == null) return 0;
    Matcher m = INTEGER.matcher(s);
    if (!m.find()) return 0;
    try {
      return Long.parseLong(m.group(1));
    } catch (NumberFormatException e) {
      return m.group(1).startsWith("-") ? Long.MIN_VALUE : Long.MAX_VALUE;
    }
  }

  public static int atoi(String s) {
    return (int) atol(s);
  }
}
