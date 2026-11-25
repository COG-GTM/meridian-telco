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

  @Override
  public List<UsageRecorded> publish(List<UsageRecorded> records) {
    List<CompletableFuture<SendResult<String, UsageRecorded>>> sent = new ArrayList<>(records.size());
    for (UsageRecorded r : records) {
      sent.add(kafka.send(Topics.USAGE_V1, r.key(), r));
    }
    kafka.flush();
    List<UsageRecorded> acked = new ArrayList<>(records.size());
    for (int i = 0; i < sent.size(); i++) {
      try {
        sent.get(i).get(ackTimeout.toMillis(), TimeUnit.MILLISECONDS);
        acked.add(records.get(i));
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
        break;
      } catch (Exception e) {
        log.warn("usage record {} not published: {}", records.get(i).key(), e.toString());
      }
    }
    return acked;
  }

  @Override
  public boolean enabled() {
    return true;
  }
}
