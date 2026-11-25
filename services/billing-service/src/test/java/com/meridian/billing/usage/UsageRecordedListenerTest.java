package com.meridian.billing.usage;

import static org.assertj.core.api.Assertions.assertThat;
import static org.awaitility.Awaitility.await;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.meridian.billing.model.UsageRecord;
import com.meridian.billing.store.UsageRepository;
import com.meridian.contracts.events.Topics;
import com.meridian.contracts.events.UsageRecorded;
import java.time.Duration;
import java.time.Instant;
import java.util.List;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.test.context.EmbeddedKafka;

/** meridian.usage.v1 → usage_record, upserted on acctId+period. */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.NONE, properties = {
    "meridian.kafka.enabled=true",
    "spring.kafka.bootstrap-servers=${spring.embedded.kafka.brokers}",
    "spring.kafka.producer.key-serializer=org.apache.kafka.common.serialization.StringSerializer",
    "spring.kafka.producer.value-serializer=org.apache.kafka.common.serialization.StringSerializer"})
@EmbeddedKafka(partitions = 1, topics = Topics.USAGE_V1)
class UsageRecordedListenerTest {

  @Autowired KafkaTemplate<String, String> kafka;
  @Autowired ObjectMapper json;
  @Autowired UsageRepository usage;
  @Autowired JdbcClient jdbc;

  @AfterEach
  void dropTestRows() {
    jdbc.sql("DELETE FROM usage_record WHERE acct_id LIKE 'TEST-%' OR acct_id LIKE '\\_TEST-%'").update();
  }

  @Test
  void consumesUsageIdempotently() throws Exception {
    Instant t = Instant.parse("2026-08-01T02:00:00Z");
    var a = new UsageRecorded("TEST-K1", "2026-08", 2048, "usage.csv", t);
    send(a);
    send(a);
    kafka.send(Topics.USAGE_V1, "bad", "{not json").get();
    kafka.send(Topics.USAGE_V1, "TEST-K2|2026-08", "{\"acctId\":\"TEST-K2\",\"period\":\"2026-08\"}").get();
    send(new UsageRecorded("TEST-K2", "2026-08", 10, "usage.csv", t));
    send(new UsageRecorded("TEST-K1", "2026-08", 4096, "usage.csv", t.plusSeconds(5)));

    await().atMost(Duration.ofSeconds(30)).untilAsserted(() ->
        assertThat(testRows()).containsExactly(
            new UsageRecord("TEST-K1", "2026-08", 4096), new UsageRecord("TEST-K2", "2026-08", 10)));
  }

  @Test
  void rejectsOnlyUnstorableRecordsAndKeepsConsuming() throws Exception {
    Instant t = Instant.parse("2026-08-01T02:00:00Z");
    long max = UsageRecordedListener.MAX_USAGE_MB;
    send(new UsageRecorded("TEST-NUL\u0000", "2026-08", 1, "usage.csv", t));
    send(new UsageRecorded("TEST-SRC", "2026-08", 1, "usage\u0000.csv", t));
    send(new UsageRecorded("TEST-OVER", "2026-08", max + 1, "usage.csv", t));
    send(new UsageRecorded("TEST-NEG", "2026-08", -1, "usage.csv", t));
    send(new UsageRecorded("TEST-MAX", "2026-08", max, "usage.csv", t));
    send(new UsageRecorded("_TEST-U", "2026-08", 0, "usage.csv", t));
    send(new UsageRecorded("TEST-LAST", "2026-08", 7, "usage.csv", t));

    await().atMost(Duration.ofSeconds(30)).untilAsserted(() ->
        assertThat(testRows()).containsExactlyInAnyOrder(
            new UsageRecord("TEST-MAX", "2026-08", max), new UsageRecord("_TEST-U", "2026-08", 0),
            new UsageRecord("TEST-LAST", "2026-08", 7)));
  }

  private void send(UsageRecorded u) throws Exception {
    kafka.send(Topics.USAGE_V1, u.key(), json.writeValueAsString(u)).get();
  }

  private List<UsageRecord> testRows() {
    return usage.findAll().stream().filter(u -> u.acctId().startsWith("TEST-") || u.acctId().startsWith("_TEST-")).toList();
  }
}
