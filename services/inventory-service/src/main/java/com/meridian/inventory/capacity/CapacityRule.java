package com.meridian.inventory.capacity;

import org.springframework.stereotype.Component;

/**
 * mediation/capacity.cpp and mediation/locations.cpp. available = total - allocated, floored at
 * zero. There is no maintenance buffer and there must not be one.
 */
@Component
public class CapacityRule {

  public int available(int totalMbps, int allocatedMbps) {
    throw new UnsupportedOperationException("Phase 2");
  }

  public int utilizationPct(int totalMbps, int allocatedMbps) {
    throw new UnsupportedOperationException("Phase 2");
  }

  public boolean canSupport(int totalMbps, int allocatedMbps, int requestedMbps) {
    throw new UnsupportedOperationException("Phase 2");
  }
}
