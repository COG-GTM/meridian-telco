package com.meridian.billing.rules;

import static com.meridian.billing.calc.Money.d;

import com.meridian.billing.calc.Binary64;
import com.meridian.billing.calc.Money;
import com.meridian.billing.model.Account;
import com.meridian.billing.model.Invoice;
import com.meridian.billing.model.UsageRecord;
import java.math.BigDecimal;
import org.springframework.stereotype.Component;

/**
 * billing/invoice.cpp {@code compute_invoice}: one invoice for one account and one usage record.
 * The register and the /invoices API both come through here so they cannot drift.
 */
@Component
public class InvoiceComposer {

  private final RatingRule rating;
  private final ProrationRule proration;
  private final LinesRule lines;
  private final SuspensionRule suspension;
  private final PromoRule promo;
  private final LateFeeRule lateFee;
  private final DiscountsRule discounts;
  private final TaxRule tax;

  public InvoiceComposer(RatingRule rating, ProrationRule proration, LinesRule lines,
      SuspensionRule suspension, PromoRule promo, LateFeeRule lateFee, DiscountsRule discounts,
      TaxRule tax) {
    this.rating = rating;
    this.proration = proration;
    this.lines = lines;
    this.suspension = suspension;
    this.promo = promo;
    this.lateFee = lateFee;
    this.discounts = discounts;
    this.tax = tax;
  }

  /** The rule set wired by hand, for the file-based billing-run that runs without Spring. */
  public static InvoiceComposer standard() {
    return new InvoiceComposer(new RatingRule(), new ProrationRule(), new LinesRule(),
        new SuspensionRule(), new PromoRule(), new LateFeeRule(), new DiscountsRule(), new TaxRule());
  }

  public Invoice compose(Account a, UsageRecord u) {
    long usageGbRated = rating.usageGbRounded(u.usageMb());
    long overageGb = rating.overageGb(u.usageMb(), a.includedGb());

    BigDecimal planCharge = proration.proratedPlanCharge(a.planFee(), a.prevPlanFee(), a.planChgDay());
    BigDecimal lineDiscount = lines.multiLineDiscount(planCharge, a.lineCnt());
    BigDecimal recurring = Money.round(d(planCharge).subtract(d(lineDiscount)));
    BigDecimal overageCharges = Money.round(rating.rateOverage(u.usageMb(), a.includedGb()));
    BigDecimal suspensionCredit = suspension.credit(a.planFee(), a.suspStart(), a.suspEnd());
    BigDecimal promoCredit = promo.credit(a.promoAmt(), a.promoDt(), u.period());
    BigDecimal lateFeeAmt = lateFee.lateFee(a.priorBal(), a.priorDue(), u.period());

    Binary64 sum = d(recurring).add(d(overageCharges)).add(d(lateFeeAmt))
        .subtract(d(suspensionCredit)).subtract(d(promoCredit));
    if (sum.signum() < 0) sum = Binary64.ZERO;
    BigDecimal subtotal = Money.round(sum);

    TaxRates rates = tax.ratesFor(a.province());
    BigDecimal loyalty = discounts.loyaltyDiscount(subtotal, a.loyaltyPct());
    BigDecimal federalTax = tax.federalTax(subtotal, rates);
    BigDecimal provincialTax = tax.provincialTax(subtotal, loyalty, rates);
    BigDecimal total = Money.round(
        d(subtotal).subtract(d(loyalty)).add(d(federalTax)).add(d(provincialTax)));

    return new Invoice(a.acctId(), a.billingRef(), u.period(), a.province(), u.usageMb(),
        usageGbRated, overageGb, planCharge, lineDiscount, recurring, overageCharges,
        suspensionCredit, promoCredit, lateFeeAmt, subtotal, loyalty, federalTax, provincialTax,
        rates.federalLabel(), rates.provincialLabel(), total);
  }
}
