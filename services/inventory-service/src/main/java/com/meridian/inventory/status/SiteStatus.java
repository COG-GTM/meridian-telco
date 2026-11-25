package com.meridian.inventory.status;

/**
 * mediation/status.cpp. 1 ACTIVE, 2 PLANNED, 3 DECOMMISSIONED (field techs also use 3 for a site
 * that is down for a while). Anything else is UNKNOWN.
 */
public final class SiteStatus {

  public static final int ACTIVE = 1;
  public static final int PLANNED = 2;
  public static final int DECOMMISSIONED = 3;

  private SiteStatus() {}

  public static String label(int statusCd) {
    return switch (statusCd) {
      case ACTIVE -> "ACTIVE";
      case PLANNED -> "PLANNED";
      case DECOMMISSIONED -> "DECOMMISSIONED";
      default -> "UNKNOWN";
    };
  }

  public static boolean isCountable(int statusCd) {
    return statusCd == ACTIVE || statusCd == PLANNED;
  }
}
