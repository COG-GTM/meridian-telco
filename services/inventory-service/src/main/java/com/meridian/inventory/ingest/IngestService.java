package com.meridian.inventory.ingest;

import com.meridian.contracts.events.UsageRecorded;
import com.meridian.inventory.events.UsagePublisher;
import com.meridian.inventory.ingest.MediationRun.IngestSummary;
import com.meridian.inventory.store.InventoryStore;
import java.nio.file.Path;
import java.time.Instant;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.locks.ReentrantLock;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.transaction.support.TransactionTemplate;

/**
 * The nightly mediation run (mediation/ingest.cpp) against the inventory store: sites and
 * circuits are rebuilt, the serviceable locations replaced, monthly usage upserted and then
 * published to billing as UsageRecorded.
 */
@Service
public class IngestService {

  public static final String USAGE_SOURCE = "mediation:usage.csv";

  private static final Logger log = LoggerFactory.getLogger(IngestService.class);

  private final InventoryStore store;
  private final UsagePublisher publisher;
  private final TransactionTemplate tx;
  private final Path dataDir;
  private final ReentrantLock running = new ReentrantLock();

  public IngestService(InventoryStore store, UsagePublisher publisher, TransactionTemplate tx,
      @Value("${meridian.data-dir}") Path dataDir) {
    this.store = store;
    this.publisher = publisher;
    this.tx = tx;
    this.dataDir = dataDir;
  }

  public record IngestResult(IngestSummary summary, int locations, int usage, int usagePublished) {}

  public Path dataDir() {
    return dataDir;
  }

  public Optional<IngestResult> run() {
    return run(dataDir);
  }

  /** Empty when another run is in progress or the drop has no site telemetry (store untouched). */
  public Optional<IngestResult> run(Path dir) {
    if (!running.tryLock()) {
      log.warn("mediation run already in progress, skipping {}", dir);
      return Optional.empty();
    }
    try {
      Instant started = Instant.now();
      MediationDrop drop = MediationDrop.load(dir);
      if (drop.siteRows().isEmpty()) {
        log.error("no site telemetry found in {}", dir);
        return Optional.empty();
      }
      IngestSummary summary = MediationRun.summarize(drop);
      List<MediationDrop.Usage> usage = drop.usage();
      List<MediationDrop.Location> locations = drop.locations();
      tx.executeWithoutResult(s -> {
        long id = store.startRun(dir.toString(), started);
        store.replaceSitesAndCircuits(drop.sites(), drop.circuits());
        store.replaceLocations(locations);
        store.upsertUsage(usage, USAGE_SOURCE, started);
        store.finishRun(id, Instant.now(), summary.sitesLoaded(), summary.circuitsLoaded(),
            locations.size(), usage.size(), summary.legacyReport());
      });
      int published = publish(usage, started);
      log.info("{}  locations loaded  : {}\n  usage records     : {} ({} published)", summary.legacyReport(),
          locations.size(), usage.size(), published);
      return Optional.of(new IngestResult(summary, locations.size(), usage.size(), published));
    } finally {
      running.unlock();
    }
  }

  private int publish(List<MediationDrop.Usage> usage, Instant recordedAt) {
    if (!publisher.enabled() || usage.isEmpty()) return 0;
    List<UsageRecorded> events = usage.stream()
        .map(u -> new UsageRecorded(u.acctId(), u.period(), u.usageMb(), USAGE_SOURCE, recordedAt))
        .toList();
    List<UsageRecorded> acked = publisher.publish(events);
    store.markPublished(acked.stream().map(e -> new String[] {e.acctId(), e.period()}).toList(), Instant.now());
    return acked.size();
  }
}
