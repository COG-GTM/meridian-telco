package com.meridian.billing.rules;

import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/** billing/promo.cpp: promo credits expire at the end of the cycle they were issued in. */
@Component
public class PromoRule {

  public boolean isLive(String issuedOn, String period) {
    throw new UnsupportedOperationException("Phase 3");
  }

  public BigDecimal credit(BigDecimal amount, String issuedOn, String period) {
    throw new UnsupportedOperationException("Phase 3");
  }
}
