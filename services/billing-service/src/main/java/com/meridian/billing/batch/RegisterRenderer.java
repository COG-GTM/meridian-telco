package com.meridian.billing.batch;

import com.meridian.billing.calc.Binary64;
import com.meridian.billing.model.Account;
import com.meridian.billing.model.Invoice;
import com.meridian.billing.model.UsageRecord;
import com.meridian.billing.rules.AccountsRule;
import com.meridian.billing.rules.InvoiceComposer;
import java.math.BigDecimal;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * billing/run_billing.cpp: the invoice register. Output is byte-identical to the legacy
 * {@code bin/billing-run [--period P] [--json] [--accounts DIR] [--usage CSV]}.
 *
 * <p>Invoices are composed in parallel, one virtual thread per usage record, and collected in
 * usage-file order so the register (and its running grand total) stays deterministic.
 */
public final class RegisterRenderer {

  /** One register row: a billed invoice, or a usage record with no account master. */
  public sealed interface Line permits Billed, NoAccount {}

  public record Billed(Invoice invoice, Account account) implements Line {}

  public record NoAccount(UsageRecord usage) implements Line {}

  /** A rendered register: the printed text plus what it was built from. */
  public record Register(String text, List<Line> lines, BigDecimal grandTotal) {
    public List<Invoice> invoices() {
      return lines.stream().<Invoice>mapMulti((l, out) -> {
        if (l instanceof Billed b) out.accept(b.invoice());
      }).toList();
    }
  }

  private static final String HEADER = "%-16s %-8s %-4s %12s %10s %12s %10s %12s\n";

  private RegisterRenderer() {}

  /** @param period YYYY-MM, or null for every period in the usage file */
  public static String render(Path accountsDir, Path usageCsv, String period, boolean json) {
    return render(InvoiceComposer.standard(), AccountsRule.loadAccounts(accountsDir),
        AccountsRule.loadUsage(usageCsv), period, json).text();
  }

  public static Register render(InvoiceComposer composer, Collection<Account> accounts,
      List<UsageRecord> usage, String period, boolean json) {
    List<Line> lines = compose(composer, AccountsRule.index(accounts), usage, period);
    StringBuilder out = new StringBuilder(lines.size() * 200);
    Binary64 grand = Binary64.ZERO;
    if (!json) {
      out.append(fmt(HEADER, "ACCT_ID", "PERIOD", "PRV", "USAGE_MB", "OVER_GB", "SUBTOTAL", "TAX", "TOTAL"));
    }
    for (Line line : lines) {
      switch (line) {
        case NoAccount(UsageRecord u) -> {
          if (!json) {
            out.append(fmt("%-16s %-8s %-4s %12d %10s %12s %10s %12s\n", u.acctId(), u.period(), "-",
                u.usageMb(), "-", "-", "-", "NO ACCOUNT"));
          }
        }
        case Billed(Invoice inv, Account a) -> {
          grand = grand.add(Binary64.of(inv.total()));
          if (json) {
            out.append(jsonLine(inv, a));
          } else {
            BigDecimal taxCol = Binary64.of(inv.federalTax()).add(Binary64.of(inv.provincialTax())).printf(2);
            out.append(fmt("%-16s %-8s %-4s %12d %10d %12s %10s %12s\n", inv.acctId(), inv.period(),
                a.province(), inv.usageMb(), inv.overageGb(), inv.subtotal().toPlainString(),
                taxCol.toPlainString(), inv.total().toPlainString()));
          }
        }
      }
    }
    BigDecimal grandTotal = grand.printf(2);
    if (!json) {
      out.append(fmt(HEADER, "TOTAL", "", "", "", "", "", "", grandTotal.toPlainString()));
    }
    return new Register(out.toString(), lines, grandTotal);
  }

  private static List<Line> compose(InvoiceComposer composer, Map<String, Account> byId,
      List<UsageRecord> usage, String period) {
    List<UsageRecord> selected = usage.stream()
        .filter(u -> period == null || period.isEmpty() || u.period().equals(period))
        .toList();
    try (ExecutorService pool = Executors.newVirtualThreadPerTaskExecutor()) {
      List<Future<Line>> futures = new ArrayList<>(selected.size());
      for (UsageRecord u : selected) {
        futures.add(pool.submit(() -> {
          Account a = byId.get(u.acctId());
          return a == null ? new NoAccount(u) : new Billed(composer.compose(a, u), a);
        }));
      }
      List<Line> lines = new ArrayList<>(futures.size());
      for (Future<Line> f : futures) lines.add(f.get());
      return lines;
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new IllegalStateException("register generation interrupted", e);
    } catch (ExecutionException e) {
      throw new IllegalStateException("invoice composition failed", e.getCause());
    }
  }

  /** run_billing.cpp print_json; cust_nm is written raw, exactly as the C++ did. */
  static String jsonLine(Invoice inv, Account a) {
    return "{\"billing_ref\":\"" + inv.billingRef() + "\",\"acct_id\":\"" + inv.acctId()
        + "\",\"period\":\"" + inv.period() + "\",\"province\":\"" + inv.province()
        + "\",\"usage_mb\":" + inv.usageMb() + ",\"overage_gb\":" + inv.overageGb()
        + ",\"plan_charge\":" + c(inv.planCharge()) + ",\"line_discount\":" + c(inv.lineDiscount())
        + ",\"overage_charges\":" + c(inv.overageCharges())
        + ",\"suspension_credit\":" + c(inv.suspensionCredit())
        + ",\"promo_credit\":" + c(inv.promoCredit()) + ",\"late_fee\":" + c(inv.lateFee())
        + ",\"subtotal\":" + c(inv.subtotal()) + ",\"loyalty\":" + c(inv.loyalty())
        + ",\"federal_tax\":" + c(inv.federalTax()) + ",\"provincial_tax\":" + c(inv.provincialTax())
        + ",\"total\":" + c(inv.total()) + ",\"cust_nm\":\"" + a.custNm() + "\"}\n";
  }

  private static String c(BigDecimal cents) {
    return cents.toPlainString();
  }

  private static String fmt(String pattern, Object... args) {
    return String.format(Locale.ROOT, pattern, args);
  }
}
