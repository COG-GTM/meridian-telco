package com.meridian.billing.usage;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.meridian.billing.store.UsageRepository;
import com.meridian.contracts.events.Topics;
import com.meridian.contracts.events.UsageRecorded;
import java.util.regex.Pattern;
import org.apache.kafka.clients.consumer.ConsumerRecord;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

/**
 * Consumes {@link UsageRecorded} from {@value Topics#USAGE_V1} into {@code usage_record}.
 * Only active with {@code meridian.kafka.enabled=true}; otherwise the service needs only Postgres.
 */
@Component
@ConditionalOnProperty(name = "meridian.kafka.enabled", havingValue = "true")
public class UsageRecordedListener {

  private static final Logger log = LoggerFactory.getLogger(UsageRecordedListener.class);
  private static final Pattern PERIOD = Pattern.compile("\\d{4}-\\d{2}");

  private final UsageRepository usage;
  private final ObjectMapper json;

  public UsageRecordedListener(UsageRepository usage, ObjectMapper json) {
    this.usage = usage;
    this.json = json;
  }

  @KafkaListener(topics = Topics.USAGE_V1, groupId = "${meridian.kafka.group-id:billing-service}")
  public void onUsage(ConsumerRecord<String, String> rec) {
    UsageRecorded u;
    try {
      u = json.readValue(rec.value(), UsageRecorded.class);
    } catch (JsonProcessingException e) {
      log.warn("skipping unreadable {} record at offset {}: {}", rec.topic(), rec.offset(), e.getMessage());
      return;
    }
    if (u.acctId() == null || u.acctId().isBlank() || u.period() == null
        || !PERIOD.matcher(u.period()).matches() || u.usageMb() < 0) {
      log.warn("skipping invalid {} record at offset {}: {}", rec.topic(), rec.offset(), u);
      return;
    }
    usage.upsert(u);
  }
}
