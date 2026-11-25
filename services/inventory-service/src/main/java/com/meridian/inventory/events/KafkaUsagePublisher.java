package com.meridian.inventory.events;

import com.meridian.contracts.events.Topics;
import com.meridian.contracts.events.UsageRecorded;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.support.SendResult;

/** One UsageRecorded per usage row, keyed acctId|period so a later record replaces the earlier one. */
public class KafkaUsagePublisher implements UsagePublisher {

  private static final Logger log = LoggerFactory.getLogger(KafkaUsagePublisher.class);

  private final KafkaTemplate<String, UsageRecorded> kafka;
  private final Duration ackTimeout;

  public KafkaUsagePublisher(KafkaTemplate<String, UsageRecorded> kafka, Duration ackTimeout) {
    this.kafka = kafka;
    this.ackTimeout = ackTimeout;
  }

  /**
   * Sends until the batch deadline passes, then waits for acknowledgements against that same
   * deadline. Whatever is not acknowledged stays unpublished in Postgres for the next run.
   */
  @Override
  public List<UsageRecorded> publish(List<UsageRecorded> records) {
    long deadline = System.nanoTime() + ackTimeout.toNanos();
    List<CompletableFuture<SendResult<String, UsageRecorded>>> sent = new ArrayList<>(records.size());
    for (UsageRecorded r : records) {
      if (System.nanoTime() - deadline >= 0) break;
      try {
        sent.add(kafka.send(Topics.USAGE_V1, r.key(), r));
      } catch (RuntimeException e) {
        sent.add(CompletableFuture.failedFuture(e));
      }
    }
    List<UsageRecorded> acked = new ArrayList<>(records.size());
    Exception firstError = null;
    for (int i = 0; i < sent.size(); i++) {
      try {
        sent.get(i).get(Math.max(0, deadline - System.nanoTime()), TimeUnit.NANOSECONDS);
        acked.add(records.get(i));
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
        break;
      } catch (Exception e) {
        if (firstError == null) firstError = e;
      }
    }
    if (acked.size() < records.size()) {
      log.warn("{} of {} usage records not acknowledged within {}, left for the next run: {}",
          records.size() - acked.size(), records.size(), ackTimeout,
          firstError == null ? "deadline passed" : firstError.toString());
    }
    return acked;
  }

  @Override
  public boolean enabled() {
    return true;
  }
}
