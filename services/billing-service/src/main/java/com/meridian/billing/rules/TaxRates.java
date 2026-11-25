package com.meridian.billing.rules;

import java.math.BigDecimal;

/** Mirrors {@code TaxRates} in billing/tax.h. Labels are "" when the province has no such line. */
public record TaxRates(BigDecimal federalPct, BigDecimal provincialPct, String federalLabel, String provincialLabel) {}
