package com.meridian.inventory.events;

import com.meridian.contracts.events.Topics;
import com.meridian.contracts.events.UsageRecorded;
import java.time.Duration;
import java.util.List;
import org.apache.kafka.clients.admin.NewTopic;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.config.TopicBuilder;
import org.springframework.kafka.core.KafkaTemplate;

/** Kafka is off unless meridian.kafka.enabled=true, so the service runs with only its Postgres. */
@Configuration(proxyBeanMethods = false)
public class UsageFeedConfig {

  @Configuration(proxyBeanMethods = false)
  @ConditionalOnProperty(name = "meridian.kafka.enabled", havingValue = "true")
  static class Kafka {

    @Bean
    UsagePublisher kafkaUsagePublisher(KafkaTemplate<String, UsageRecorded> kafka,
        @Value("${meridian.kafka.ack-timeout:30s}") Duration ackTimeout) {
      return new KafkaUsagePublisher(kafka, ackTimeout);
    }

    @Bean
    NewTopic usageTopic(@Value("${meridian.kafka.usage-topic.partitions:3}") int partitions,
        @Value("${meridian.kafka.usage-topic.replicas:1}") int replicas) {
      return TopicBuilder.name(Topics.USAGE_V1).partitions(partitions).replicas(replicas).compact().build();
    }
  }

  @Bean
  @ConditionalOnProperty(name = "meridian.kafka.enabled", havingValue = "false", matchIfMissing = true)
  UsagePublisher disabledUsagePublisher() {
    return new UsagePublisher() {
      @Override
      public List<UsageRecorded> publish(List<UsageRecorded> records) {
        return List.of();
      }

      @Override
      public boolean enabled() {
        return false;
      }
    };
  }
}
