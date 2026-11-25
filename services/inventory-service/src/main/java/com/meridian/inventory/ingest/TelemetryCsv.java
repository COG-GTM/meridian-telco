package com.meridian.inventory.ingest;

import java.nio.file.Path;
import java.util.List;
import java.util.Map;

/**
 * Reads a telemetry drop (data/*.csv) the way mediation/csv.h does: the header row names the
 * fields, each row becomes field name to raw text. Phase 2 backs this with a standard CSV library.
 */
public final class TelemetryCsv {

  private TelemetryCsv() {}

  public static List<Map<String, String>> read(Path csv) {
    throw new UnsupportedOperationException("Phase 2");
  }

  /** atoi semantics of csv.h to_int: leading whitespace and sign, digits up to the first non-digit, else 0. */
  public static int toInt(String s) {
    throw new UnsupportedOperationException("Phase 2");
  }
}
