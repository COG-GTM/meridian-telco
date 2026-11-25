package com.meridian.billing.batch;

import java.nio.file.Path;

/**
 * Seam for the billing-run register. Must produce output byte-identical to the legacy
 * {@code bin/billing-run [--period P] [--json] [--accounts DIR] [--usage CSV]}.
 */
public final class RegisterRenderer {

  private RegisterRenderer() {}

  /** @param period YYYY-MM, or null for every period in the usage file */
  public static String render(Path accountsDir, Path usageCsv, String period, boolean json) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
