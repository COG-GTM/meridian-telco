package com.meridian.billing.model;

import java.math.BigDecimal;

/** billing/invoice.h {@code Invoice}. Every amount is cents (scale 2). */
public record Invoice(
    String acctId,
    String billingRef,
    String period,
    String province,
    long usageMb,
    long usageGbRated,
    long overageGb,
    BigDecimal planCharge,
    BigDecimal lineDiscount,
    BigDecimal recurring,
    BigDecimal overageCharges,
    BigDecimal suspensionCredit,
    BigDecimal promoCredit,
    BigDecimal lateFee,
    BigDecimal subtotal,
    BigDecimal loyalty,
    BigDecimal federalTax,
    BigDecimal provincialTax,
    String federalLabel,
    String provincialLabel,
    BigDecimal total) {}
