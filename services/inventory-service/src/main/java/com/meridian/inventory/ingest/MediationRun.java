package com.meridian.inventory.ingest;

import java.nio.file.Path;

/** One mediation ingest over a data/ drop (mediation/ingest.cpp). */
public final class MediationRun {

  private MediationRun() {}

  /** Summary that bin/mediation prints, computed from dataDir/sites.csv and dataDir/circuits.csv. */
  public static IngestSummary summarize(Path dataDir) {
    throw new UnsupportedOperationException("Phase 2");
  }

  public record IngestSummary(int sitesLoaded, int countableSites, int circuitsLoaded,
      int activeCircuits, int activeCapacityMbps, int availableCapacityMbps, int utilizationPct) {

    /** The exact stdout of bin/mediation (contracts/mediation/mediation-run.txt). */
    public String legacyReport() {
      return "mediation run complete\n"
          + "  sites loaded      : " + sitesLoaded + "\n"
          + "  countable sites   : " + countableSites + "\n"
          + "  circuits loaded   : " + circuitsLoaded + "\n"
          + "  active circuits   : " + activeCircuits + "\n"
          + "  active capacity   : " + activeCapacityMbps + " mbps\n"
          + "  available capacity: " + availableCapacityMbps + " mbps\n"
          + "  utilization       : " + utilizationPct + "%\n";
    }
  }
}
