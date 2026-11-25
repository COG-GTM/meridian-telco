package com.meridian.inventory.ingest;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.SmartLifecycle;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

/** Legacy cron was "0 2 * * *"; meridian.ingest.cron overrides it. */
@Component
public class IngestScheduler implements SmartLifecycle {

  private final IngestService ingest;
  private final boolean onStartup;
  private volatile boolean running;

  public IngestScheduler(IngestService ingest, @Value("${meridian.ingest.on-startup:false}") boolean onStartup) {
    this.ingest = ingest;
    this.onStartup = onStartup;
  }

  @Scheduled(cron = "${meridian.ingest.cron:0 0 2 * * *}", zone = "${meridian.ingest.zone:}")
  public void nightly() {
    ingest.run();
  }

  /**
   * As inventory-api loaded locations.csv before serving: the locations (or, with
   * meridian.ingest.on-startup, the whole drop) are in Postgres before :8081 opens. Runs as a
   * lifecycle phase so KafkaAdmin has already provisioned the usage topic.
   */
  @Override
  public void start() {
    if (!onStartup || ingest.run().isEmpty()) ingest.refreshLocations();
    running = true;
  }

  @Override
  public void stop() {
    running = false;
  }

  @Override
  public boolean isRunning() {
    return running;
  }

  /** Before the web server's lifecycle (DEFAULT_PHASE - 1024) starts listening. */
  @Override
  public int getPhase() {
    return DEFAULT_PHASE - 2048;
  }
}
