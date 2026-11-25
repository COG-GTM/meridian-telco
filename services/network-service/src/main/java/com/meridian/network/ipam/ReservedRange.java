package com.meridian.network.ipam;

/** A {@code reserve <cidr> purpose <purpose>} line of network/reserved-ranges.conf. */
public record ReservedRange(String cidr, String purpose) {}
