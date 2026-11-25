package com.meridian.migration;

import java.math.BigDecimal;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** C atof/atol semantics on the flat-file masters: longest numeric prefix, else the default. */
final class Atof {

  private static final Pattern DECIMAL = Pattern.compile("^\\s*([+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?)");
  private static final Pattern INTEGER = Pattern.compile("^\\s*([+-]?\\d+)");

  private Atof() {}

  static BigDecimal decimal(String s, String dflt) {
    if (s == null) return new BigDecimal(dflt);
    Matcher m = DECIMAL.matcher(s);
    return m.find() ? new BigDecimal(m.group(1)) : BigDecimal.ZERO;
  }

  static long integer(String s, long dflt) {
    if (s == null) return dflt;
    Matcher m = INTEGER.matcher(s);
    return m.find() ? Long.parseLong(m.group(1)) : 0;
  }
}
