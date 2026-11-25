package com.meridian.inventory.ingest;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;

/** Legacy cron was "0 2 * * *"; meridian.ingest.cron overrides it. */
@Component
public class IngestScheduler {

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

  @EventListener(ApplicationReadyEvent.class)
  public void startup() {
    if (onStartup) ingest.run();
  }
}
