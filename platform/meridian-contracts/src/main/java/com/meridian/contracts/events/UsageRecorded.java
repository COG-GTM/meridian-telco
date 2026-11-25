package com.meridian.contracts.events;

import java.time.Instant;

/**
 * One monthly usage total for one account, published by inventory-service after each
 * mediation ingest and consumed by billing-service. Keyed on {@code acctId + "|" + period};
 * a later record for the same key replaces the earlier one.
 */
public record UsageRecorded(String acctId, String period, long usageMb, String source, Instant recordedAt) {

  public String key() {
    return acctId + "|" + period;
  }
}
