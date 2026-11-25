package com.meridian.billing.usage;

import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.kafka.listener.DefaultErrorHandler;
import org.springframework.util.backoff.ExponentialBackOff;

/**
 * A usage record that cannot be stored (e.g. Postgres is down) is retried until it is, rather
 * than committed past; unreadable or invalid records are already skipped by the listener.
 */
@Configuration
@ConditionalOnProperty(name = "meridian.kafka.enabled", havingValue = "true")
class UsageFeedConfig {

  @Bean
  DefaultErrorHandler usageFeedErrorHandler() {
    ExponentialBackOff backOff = new ExponentialBackOff(500, 2.0);
    backOff.setMaxInterval(30_000);
    return new DefaultErrorHandler(backOff);
  }
}
