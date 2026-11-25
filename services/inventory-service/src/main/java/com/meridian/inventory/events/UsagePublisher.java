package com.meridian.inventory.events;

import com.meridian.contracts.events.UsageRecorded;
import java.util.List;

/** Async usage feed to billing-service (topic meridian.usage.v1). */
public interface UsagePublisher {

  /** Publishes the records and returns the ones the broker acknowledged. */
  List<UsageRecorded> publish(List<UsageRecorded> records);

  boolean enabled();
}
