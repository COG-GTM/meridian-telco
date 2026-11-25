package com.meridian.network.ipam;

import java.nio.file.Path;

/** network/ipam.cpp: management address report over network/devices.csv (10.20.0.0/16). */
public final class IpamReport {

  private IpamReport() {}

  /** The exact stdout of bin/ipam --devices devicesCsv (contracts/ipam/ipam-report.txt). */
  public static String legacyText(Path devicesCsv) {
    throw new UnsupportedOperationException("Phase 4");
  }
}
