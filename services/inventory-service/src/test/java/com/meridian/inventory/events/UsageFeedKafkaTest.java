package com.meridian.inventory.events;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.events.Topics;
import com.meridian.contracts.events.UsageRecorded;
import com.meridian.inventory.ingest.IngestService;
import com.meridian.inventory.store.InventoryStore;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.apache.kafka.clients.consumer.Consumer;
import org.apache.kafka.clients.consumer.ConsumerConfig;
import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.apache.kafka.common.serialization.StringDeserializer;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.kafka.core.DefaultKafkaConsumerFactory;
import org.springframework.kafka.support.serializer.JsonDeserializer;
import org.springframework.kafka.test.EmbeddedKafkaBroker;
import org.springframework.kafka.test.context.EmbeddedKafka;
import org.springframework.kafka.test.utils.KafkaTestUtils;

/** The startup ingest publishes one UsageRecorded per data/usage.csv row to meridian.usage.v1. */
@SpringBootTest(properties = {
    "meridian.kafka.enabled=true",
    "spring.kafka.bootstrap-servers=${spring.embedded.kafka.brokers}"})
@EmbeddedKafka(partitions = 1)
class UsageFeedKafkaTest {

  @Autowired EmbeddedKafkaBroker broker;
  @Autowired InventoryStore store;

  @Test
  void everyUsageRowIsPublishedKeyedByAccountAndPeriod() {
    Map<String, Object> props = KafkaTestUtils.consumerProps("usage-feed-test", "false", broker);
    props.put(ConsumerConfig.AUTO_OFFSET_RESET_CONFIG, "earliest");
    JsonDeserializer<UsageRecorded> values = new JsonDeserializer<>(UsageRecorded.class, false);
    List<ConsumerRecord<String, UsageRecorded>> got = new ArrayList<>();
    try (Consumer<String, UsageRecorded> consumer =
        new DefaultKafkaConsumerFactory<>(props, new StringDeserializer(), values).createConsumer()) {
      consumer.subscribe(List.of(Topics.USAGE_V1));
      long deadline = System.currentTimeMillis() + 30_000;
      while (got.size() < 400 && System.currentTimeMillis() < deadline) {
        KafkaTestUtils.getRecords(consumer, Duration.ofSeconds(2)).forEach(got::add);
      }
    }
    assertThat(got).hasSize(400);
    assertThat(got).extracting(ConsumerRecord::key).doesNotHaveDuplicates();
    // meridian.usage.v1 has several partitions, so consume order is not file order
    ConsumerRecord<String, UsageRecorded> first =
        got.stream().filter(r -> r.key().equals("BEACON-004417|2026-06")).findFirst().orElseThrow();
    assertThat(first.key()).isEqualTo("BEACON-004417|2026-06").isEqualTo(first.value().key());
    assertThat(first.value().usageMb()).isEqualTo(5813259L);
    assertThat(first.value().source()).isEqualTo(IngestService.USAGE_SOURCE);
    assertThat(first.value().recordedAt()).isNotNull();
    assertThat(store.usage()).allSatisfy(u -> assertThat(u.publishedAt()).isNotNull());
  }
}
