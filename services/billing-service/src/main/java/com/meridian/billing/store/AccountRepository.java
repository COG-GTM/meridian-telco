package com.meridian.billing.store;

import com.meridian.billing.model.Account;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

/** The {@code account} table: billing's copy of the account master (billing/accounts/*.rec). */
@Repository
public class AccountRepository {

  private static final String COLUMNS = "acct_id, billing_ref, cust_nm, tax_id, svc_addr, province, "
      + "plan_cd, plan_fee, included_gb, prev_plan_fee, plan_chg_day, line_cnt, promo_amt, promo_dt, "
      + "susp_start, susp_end, prior_bal, prior_due, loyalty_pct";

  private final JdbcClient jdbc;

  public AccountRepository(JdbcClient jdbc) {
    this.jdbc = jdbc;
  }

  public List<Account> findAll() {
    return jdbc.sql("SELECT " + COLUMNS + " FROM account ORDER BY acct_id COLLATE \"C\"")
        .query((rs, n) -> new Account(rs.getString(1), s(rs.getString(2)), s(rs.getString(3)),
            s(rs.getString(4)), s(rs.getString(5)), s(rs.getString(6)), s(rs.getString(7)),
            rs.getBigDecimal(8), rs.getLong(9), rs.getBigDecimal(10), rs.getInt(11), rs.getInt(12),
            rs.getBigDecimal(13), s(rs.getString(14)), rs.getInt(15), rs.getInt(16),
            rs.getBigDecimal(17), s(rs.getString(18)), rs.getBigDecimal(19)))
        .list();
  }

  public Optional<Account> find(String acctId) {
    return findAll().stream().filter(a -> a.acctId().equals(acctId)).findFirst();
  }

  public long count() {
    return jdbc.sql("SELECT count(*) FROM account").query(Long.class).single();
  }

  @Transactional
  public void insertAll(Collection<Account> accounts, String sourceFile) {
    for (Account a : accounts) {
      jdbc.sql("INSERT INTO account (" + COLUMNS + ", source_file) VALUES "
              + "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT (acct_id) DO NOTHING")
          .params(a.acctId(), a.billingRef(), a.custNm(), a.taxId(), a.svcAddr(), a.province(),
              a.planCd(), a.planFee(), a.includedGb(), a.prevPlanFee(), a.planChgDay(), a.lineCnt(),
              a.promoAmt(), a.promoDt(), a.suspStart(), a.suspEnd(), a.priorBal(), a.priorDue(),
              a.loyaltyPct(), sourceFile)
          .update();
    }
  }

  private static String s(String v) {
    return v == null ? "" : v;
  }

}
