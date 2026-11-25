package com.meridian.billing.store;

import com.meridian.billing.model.UsageRecord;
import com.meridian.contracts.events.UsageRecorded;
import java.sql.Timestamp;
import java.util.List;
import org.springframework.jdbc.core.simple.JdbcClient;
import org.springframework.stereotype.Repository;

/**
 * The {@code usage_record} table, fed by {@code meridian.usage.v1} (or seeded from a usage file).
 * Ordered by account then period, byte-wise, which is the order of the legacy usage drops; a
 * Kafka feed has no global order across partitions, so arrival order (seq) is not used for output.
 */
@Repository
public class UsageRepository {

  private final JdbcClient jdbc;

  public UsageRepository(JdbcClient jdbc) {
    this.jdbc = jdbc;
  }

  public List<UsageRecord> findAll() {
    return jdbc.sql("SELECT acct_id, period, usage_mb FROM usage_record "
            + "ORDER BY acct_id COLLATE \"C\", period COLLATE \"C\"")
        .query((rs, n) -> new UsageRecord(rs.getString(1), rs.getString(2), rs.getLong(3)))
        .list();
  }

  /**
   * Idempotent on (acct_id, period): a replay of the same record is a no-op, a later record for
   * the key replaces the usage, an older (out of order) one is ignored. Returns rows changed.
   */
  public int upsert(UsageRecorded u) {
    return jdbc.sql("""
            INSERT INTO usage_record (acct_id, period, seq, usage_mb, source, recorded_at)
            VALUES (?, ?, nextval('usage_record_seq'), ?, ?, ?)
            ON CONFLICT (acct_id, period) DO UPDATE
               SET usage_mb = EXCLUDED.usage_mb, source = EXCLUDED.source,
                   recorded_at = EXCLUDED.recorded_at, received_at = now()
             WHERE usage_record.recorded_at IS NULL
                OR EXCLUDED.recorded_at IS NULL
                OR EXCLUDED.recorded_at >= usage_record.recorded_at""")
        .params(u.acctId(), u.period(), u.usageMb(), u.source(),
            u.recordedAt() == null ? null : Timestamp.from(u.recordedAt()))
        .update();
  }

  /** Seeds a record without overriding anything already received from the feed. */
  public int insertIfAbsent(UsageRecord u, String source) {
    return jdbc.sql("""
            INSERT INTO usage_record (acct_id, period, seq, usage_mb, source)
            VALUES (?, ?, nextval('usage_record_seq'), ?, ?)
            ON CONFLICT (acct_id, period) DO NOTHING""")
        .params(u.acctId(), u.period(), u.usageMb(), source)
        .update();
  }

  public long count() {
    return jdbc.sql("SELECT count(*) FROM usage_record").query(Long.class).single();
  }
}
