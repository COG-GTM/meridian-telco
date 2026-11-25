package com.meridian.billing.model;

import java.math.BigDecimal;

/**
 * billing/accounts.h {@code Account}. Amounts are the decimal text of the flat record (what
 * {@code atof} parsed); the rules lift them into the legacy double arithmetic where needed.
 */
public record Account(
    String acctId,
    String billingRef,
    String custNm,
    String taxId,
    String svcAddr,
    String province,
    String planCd,
    BigDecimal planFee,
    long includedGb,
    BigDecimal prevPlanFee,
    int planChgDay,
    int lineCnt,
    BigDecimal promoAmt,
    String promoDt,
    int suspStart,
    int suspEnd,
    BigDecimal priorBal,
    String priorDue,
    BigDecimal loyaltyPct) {}
