package com.meridian.billing.model;

/** billing/accounts.h {@code UsageRec}: one monthly usage total for one account. */
public record UsageRecord(String acctId, String period, long usageMb) {}
