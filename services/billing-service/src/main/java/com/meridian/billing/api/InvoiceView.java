package com.meridian.billing.api;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.annotation.JsonPropertyOrder;
import com.meridian.billing.calc.Binary64;
import com.meridian.billing.model.Account;
import com.meridian.billing.model.Invoice;
import java.math.BigDecimal;
import java.util.List;

/** One element of the frozen /invoices contract (billing/invoice-api/main.cpp invoice_json). */
@JsonPropertyOrder({"ACCT_ID", "BILLING_REF", "CUST_NM", "TAX_ID", "SVC_ADDR", "PROVINCE", "PERIOD",
    "PLAN_CD", "USAGE_MB", "USAGE_GB_RATED", "INCLUDED_GB", "OVERAGE_GB", "PLAN_CHARGE",
    "LINE_DISCOUNT", "RECURRING", "OVERAGE_CHARGES", "SUSPENSION_CREDIT", "PROMO_CREDIT", "LATE_FEE",
    "SUBTOTAL", "LOYALTY_PCT", "LOYALTY", "FED_TAX_LBL", "FED_TAX", "PROV_TAX_LBL", "PROV_TAX",
    "INVOICE_TOTAL"})
public record InvoiceView(
    @JsonProperty("ACCT_ID") String acctId,
    @JsonProperty("BILLING_REF") String billingRef,
    @JsonProperty("CUST_NM") String custNm,
    @JsonProperty("TAX_ID") String taxId,
    @JsonProperty("SVC_ADDR") String svcAddr,
    @JsonProperty("PROVINCE") String province,
    @JsonProperty("PERIOD") String period,
    @JsonProperty("PLAN_CD") String planCd,
    @JsonProperty("USAGE_MB") long usageMb,
    @JsonProperty("USAGE_GB_RATED") long usageGbRated,
    @JsonProperty("INCLUDED_GB") long includedGb,
    @JsonProperty("OVERAGE_GB") long overageGb,
    @JsonProperty("PLAN_CHARGE") BigDecimal planCharge,
    @JsonProperty("LINE_DISCOUNT") BigDecimal lineDiscount,
    @JsonProperty("RECURRING") BigDecimal recurring,
    @JsonProperty("OVERAGE_CHARGES") BigDecimal overageCharges,
    @JsonProperty("SUSPENSION_CREDIT") BigDecimal suspensionCredit,
    @JsonProperty("PROMO_CREDIT") BigDecimal promoCredit,
    @JsonProperty("LATE_FEE") BigDecimal lateFee,
    @JsonProperty("SUBTOTAL") BigDecimal subtotal,
    @JsonProperty("LOYALTY_PCT") BigDecimal loyaltyPct,
    @JsonProperty("LOYALTY") BigDecimal loyalty,
    @JsonProperty("FED_TAX_LBL") String fedTaxLbl,
    @JsonProperty("FED_TAX") BigDecimal fedTax,
    @JsonProperty("PROV_TAX_LBL") String provTaxLbl,
    @JsonProperty("PROV_TAX") BigDecimal provTax,
    @JsonProperty("INVOICE_TOTAL") BigDecimal invoiceTotal) {

  public static InvoiceView of(Invoice i, Account a) {
    return new InvoiceView(a.acctId(), a.billingRef(), a.custNm(), a.taxId(), a.svcAddr(),
        a.province(), i.period(), a.planCd(), i.usageMb(), i.usageGbRated(), a.includedGb(),
        i.overageGb(), i.planCharge(), i.lineDiscount(), i.recurring(), i.overageCharges(),
        i.suspensionCredit(), i.promoCredit(), i.lateFee(), i.subtotal(),
        Binary64.of(a.loyaltyPct()).printf(2), i.loyalty(), i.federalLabel(), i.federalTax(),
        i.provincialLabel(), i.provincialTax(), i.total());
  }

  public record Page(@JsonProperty("invoices") List<InvoiceView> invoices) {}
}
