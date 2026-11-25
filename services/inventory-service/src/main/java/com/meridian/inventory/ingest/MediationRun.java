package com.meridian.inventory.ingest;

import com.meridian.inventory.capacity.CapacityRule;
import com.meridian.inventory.status.CircuitCounter;
import com.meridian.inventory.status.SiteStatus;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;

/** One mediation ingest over a data/ drop (mediation/ingest.cpp). */
public final class MediationRun {

  private static final CapacityRule CAPACITY = new CapacityRule();

  private MediationRun() {}

  /** Summary that bin/mediation prints, computed from dataDir/sites.csv and dataDir/circuits.csv. */
  public static IngestSummary summarize(Path dataDir) {
    return summarize(TelemetryCsv.read(dataDir.resolve("sites.csv")),
        TelemetryCsv.read(dataDir.resolve("circuits.csv")));
  }

  public static IngestSummary summarize(MediationDrop drop) {
    return summarize(drop.siteRows(), drop.circuitRows());
  }

  static IngestSummary summarize(List<Map<String, String>> sites, List<Map<String, String>> circuits) {
    int countable = 0;
    long total = 0;
    long alloc = 0;
    for (Map<String, String> r : sites) {
      if (SiteStatus.isCountable(TelemetryCsv.toInt(r.getOrDefault("STATUS_CD", "")))) countable++;
      total += TelemetryCsv.toInt(r.getOrDefault("TOTAL_CAP_MBPS", ""));
      alloc += TelemetryCsv.toInt(r.getOrDefault("ALLOC_CAP_MBPS", ""));
    }
    java.util.function.ToIntFunction<Map<String, String>> status =
        r -> TelemetryCsv.toInt(r.getOrDefault("STATUS_CD", ""));
    return new IngestSummary(sites.size(), countable, circuits.size(),
        CircuitCounter.countActive(circuits, status),
        CircuitCounter.sumActiveCapacity(circuits, status,
            r -> TelemetryCsv.toInt(r.getOrDefault("CAP_MBPS", ""))),
        CAPACITY.available((int) total, (int) alloc),
        CAPACITY.utilizationPct((int) total, (int) alloc));
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
