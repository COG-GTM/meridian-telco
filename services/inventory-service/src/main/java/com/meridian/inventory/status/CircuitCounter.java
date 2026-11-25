package com.meridian.inventory.status;

import java.util.function.ToIntFunction;

/**
 * mediation/circuit_counter.cpp. A circuit is active when STATUS_CD is 1; role does not matter
 * (a standby circuit is still a circuit we own).
 */
public final class CircuitCounter {

  public static final int ROLE_PRIMARY = 1;
  public static final int ROLE_STANDBY = 2;
  public static final int ROLE_FAILOVER = 3;

  private CircuitCounter() {}

  public static <T> int countActive(Iterable<T> circuits, ToIntFunction<T> statusCd) {
    int n = 0;
    for (T c : circuits) {
      if (statusCd.applyAsInt(c) == 1) n++;
    }
    return n;
  }

  public static <T> int sumActiveCapacity(Iterable<T> circuits, ToIntFunction<T> statusCd,
      ToIntFunction<T> capMbps) {
    int total = 0;
    for (T c : circuits) {
      if (statusCd.applyAsInt(c) == 1) total += capMbps.applyAsInt(c);
    }
    return total;
  }
}
