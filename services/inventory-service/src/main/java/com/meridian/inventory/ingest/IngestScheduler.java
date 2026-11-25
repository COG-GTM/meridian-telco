package com.meridian.inventory.ingest;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.beans.factory.SmartInitializingSingleton;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

/** Legacy cron was "0 2 * * *"; meridian.ingest.cron overrides it. */
@Component
public class IngestScheduler implements SmartInitializingSingleton {

  private final IngestService ingest;
  private final boolean onStartup;

  public IngestScheduler(IngestService ingest, @Value("${meridian.ingest.on-startup:false}") boolean onStartup) {
    this.ingest = ingest;
    this.onStartup = onStartup;
  }

  @Scheduled(cron = "${meridian.ingest.cron:0 0 2 * * *}", zone = "${meridian.ingest.zone:}")
  public void nightly() {
    ingest.run();
  }

  /**
   * Before the web server opens :8081, as inventory-api loaded locations.csv before serving:
   * the locations (or, with meridian.ingest.on-startup, the whole drop) are in Postgres first.
   */
  @Override
  public void afterSingletonsInstantiated() {
    if (!onStartup || ingest.run().isEmpty()) ingest.refreshLocations();
  }
}
