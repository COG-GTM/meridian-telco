package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/**
 * billing/rating.cpp: usage arrives in MB, billing is in whole GB with partial gigs rounded up,
 * $10 a gig over the plan allowance, flat, no tiers.
 */
@Component
public class RatingRule {

  public static final BigDecimal OVERAGE_RATE_PER_GB = new BigDecimal("10.00");

  public long usageGbRounded(long usageMb) {
    long gb = usageMb / 1024;
    if (usageMb % 1024 != 0) gb++;
    return gb;
  }

  public long overageGb(long usageMb, long includedGb) {
    long gb = usageGbRounded(usageMb);
    return gb <= includedGb ? 0 : gb - includedGb;
  }

  /** Unrounded overage amount; whole GB times a whole-dollar rate, exact in the legacy double too. */
  public BigDecimal rateOverage(long usageMb, long includedGb) {
    return BigDecimal.valueOf(overageGb(usageMb, includedGb)).multiply(OVERAGE_RATE_PER_GB);
  }
}
