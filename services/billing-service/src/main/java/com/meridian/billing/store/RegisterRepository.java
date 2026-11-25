package com.meridian.billing.store;

import com.meridian.billing.model.Invoice;
import java.math.BigDecimal;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

/** {@code register_run} and {@code invoice}: what a billing run produced, previously only printed. */
@Repository
public class RegisterRepository {

  private final JdbcClient jdbc;

  public RegisterRepository(JdbcClient jdbc) {
    this.jdbc = jdbc;
  }

  public long save(String period, Instant started, Instant finished, List<Invoice> invoices,
      BigDecimal grandTotal) {
    long id = jdbc.sql("INSERT INTO register_run (period, started_at, finished_at, invoice_count, "
            + "grand_total) VALUES (?, ?, ?, ?, ?) RETURNING id")
        .params(period, Timestamp.from(started), Timestamp.from(finished), invoices.size(), grandTotal)
        .query(Long.class).single();
    for (Invoice i : invoices) {
      jdbc.sql("INSERT INTO invoice (register_run_id, acct_id, period, billing_ref, province, usage_mb, "
              + "usage_gb_rated, overage_gb, plan_charge, line_discount, recurring, overage_charges, "
              + "suspension_credit, promo_credit, late_fee, subtotal, loyalty, federal_tax, provincial_tax, "
              + "federal_label, provincial_label, total) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")
          .params(id, i.acctId(), i.period(), i.billingRef(), i.province(), i.usageMb(),
              i.usageGbRated(), i.overageGb(), i.planCharge(), i.lineDiscount(), i.recurring(),
              i.overageCharges(), i.suspensionCredit(), i.promoCredit(), i.lateFee(), i.subtotal(),
              i.loyalty(), i.federalTax(), i.provincialTax(), i.federalLabel(), i.provincialLabel(),
              i.total())
          .update();
    }
    return id;
  }

  public List<Invoice> invoices(long registerRunId) {
    return jdbc.sql("SELECT acct_id, period, billing_ref, province, usage_mb, usage_gb_rated, overage_gb, "
            + "plan_charge, line_discount, recurring, overage_charges, suspension_credit, promo_credit, "
            + "late_fee, subtotal, loyalty, federal_tax, provincial_tax, federal_label, provincial_label, "
            + "total FROM invoice WHERE register_run_id = ? ORDER BY acct_id COLLATE \"C\", period COLLATE \"C\"")
        .param(registerRunId)
        .query((rs, n) -> new Invoice(rs.getString(1), rs.getString(3), rs.getString(2),
            rs.getString(4), rs.getLong(5), rs.getLong(6), rs.getLong(7), rs.getBigDecimal(8),
            rs.getBigDecimal(9), rs.getBigDecimal(10), rs.getBigDecimal(11), rs.getBigDecimal(12),
            rs.getBigDecimal(13), rs.getBigDecimal(14), rs.getBigDecimal(15), rs.getBigDecimal(16),
            rs.getBigDecimal(17), rs.getBigDecimal(18), rs.getString(19), rs.getString(20),
            rs.getBigDecimal(21)))
        .list();
  }

  public BigDecimal grandTotal(long registerRunId) {
    return jdbc.sql("SELECT grand_total FROM register_run WHERE id = ?").param(registerRunId)
        .query(BigDecimal.class).single();
  }
}
