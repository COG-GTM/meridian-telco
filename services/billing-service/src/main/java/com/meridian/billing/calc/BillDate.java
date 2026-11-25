package com.meridian.billing.calc;

/** billing/datecalc.h: dates arrive as YYYY-MM-DD out of the account records, periods as YYYY-MM. */
public record BillDate(int y, int m, int d, boolean ok) {

  private static final BillDate INVALID = new BillDate(0, 0, 0, false);

  public static BillDate parseDate(String s) {
    if (s == null || s.length() < 10) return INVALID;
    int y = CNum.atoi(s.substring(0, 4));
    int m = CNum.atoi(s.substring(5, 7));
    int d = CNum.atoi(s.substring(8, 10));
    return new BillDate(y, m, d, y > 0 && m > 0 && d > 0);
  }

  public static BillDate periodStart(String period) {
    if (period == null || period.length() < 7) return new BillDate(0, 0, 1, false);
    int y = CNum.atoi(period.substring(0, 4));
    int m = CNum.atoi(period.substring(5, 7));
    return new BillDate(y, m, 1, y > 0 && m > 0);
  }

  /** Days since 1970-01-01 (Howard Hinnant's days_from_civil). */
  public long serialDay() {
    long yy = y - (m <= 2 ? 1 : 0);
    long era = (yy >= 0 ? yy : yy - 399) / 400;
    long yoe = yy - era * 400;
    long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
  }

  public static long daysBetween(BillDate from, BillDate to) {
    return to.serialDay() - from.serialDay();
  }
}
