package com.meridian.inventory.store;

import com.meridian.inventory.ingest.MediationDrop.Circuit;
import com.meridian.inventory.ingest.MediationDrop.Location;
import com.meridian.inventory.ingest.MediationDrop.Site;
import com.meridian.inventory.ingest.MediationDrop.Usage;
import java.math.BigDecimal;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;

/** inventory-service PostgreSQL store (Flyway db/migration). Replaces mediation/store.cpp. */
@Repository
public class InventoryStore {

  private static final String SITE_COLS = "asset_id, site_nm, site_cd, region_cd, lat, lon, status_cd, "
      + "tower_reg, total_cap_mbps, alloc_cap_mbps";
  private static final String CIRCUIT_COLS = "circuit_id, circuit_nm, a_asset_id, z_asset_id, cap_mbps, "
      + "alloc_mbps, role_cd, status_cd";

  private static final RowMapper<Site> SITE = (rs, i) -> new Site(rs.getInt(1), rs.getString(2),
      rs.getString(3), rs.getString(4), rs.getDouble(5), rs.getDouble(6), rs.getInt(7),
      rs.getString(8), rs.getInt(9), rs.getInt(10));
  private static final RowMapper<Circuit> CIRCUIT = (rs, i) -> new Circuit(rs.getString(1),
      rs.getString(2), rs.getInt(3), rs.getInt(4), rs.getInt(5), rs.getInt(6), rs.getInt(7),
      rs.getInt(8));
  private static final RowMapper<Location> LOCATION = (rs, i) -> new Location(rs.getString(1),
      rs.getString(2), rs.getString(3), rs.getString(4), rs.getInt(5), rs.getInt(6));

  private final JdbcTemplate jdbc;

  public InventoryStore(JdbcTemplate jdbc) {
    this.jdbc = jdbc;
  }

  // ------------------------------------------------------------------ ingest

  /** store.cpp create_schema + load_sites + load_circuits: full replace, INSERT OR REPLACE per row. */
  public void replaceSitesAndCircuits(List<Site> sites, List<Circuit> circuits) {
    jdbc.update("DELETE FROM site");
    jdbc.update("DELETE FROM circuit");
    jdbc.batchUpdate("INSERT INTO site (" + SITE_COLS + ") VALUES (?,?,?,?,?,?,?,?,?,?) "
        + "ON CONFLICT (asset_id) DO UPDATE SET site_nm = EXCLUDED.site_nm, site_cd = EXCLUDED.site_cd, "
        + "region_cd = EXCLUDED.region_cd, lat = EXCLUDED.lat, lon = EXCLUDED.lon, "
        + "status_cd = EXCLUDED.status_cd, tower_reg = EXCLUDED.tower_reg, "
        + "total_cap_mbps = EXCLUDED.total_cap_mbps, alloc_cap_mbps = EXCLUDED.alloc_cap_mbps",
        sites, 500, (ps, s) -> {
          ps.setInt(1, s.assetId());
          ps.setString(2, s.siteNm());
          ps.setString(3, s.siteCd());
          ps.setString(4, s.regionCd());
          ps.setDouble(5, s.lat());
          ps.setDouble(6, s.lon());
          ps.setInt(7, s.statusCd());
          ps.setString(8, s.towerReg());
          ps.setInt(9, s.totalCapMbps());
          ps.setInt(10, s.allocCapMbps());
        });
    jdbc.batchUpdate("INSERT INTO circuit (" + CIRCUIT_COLS + ") VALUES (?,?,?,?,?,?,?,?) "
        + "ON CONFLICT (circuit_id) DO UPDATE SET circuit_nm = EXCLUDED.circuit_nm, "
        + "a_asset_id = EXCLUDED.a_asset_id, z_asset_id = EXCLUDED.z_asset_id, cap_mbps = EXCLUDED.cap_mbps, "
        + "alloc_mbps = EXCLUDED.alloc_mbps, role_cd = EXCLUDED.role_cd, status_cd = EXCLUDED.status_cd",
        circuits, 500, (ps, c) -> {
          ps.setString(1, c.circuitId());
          ps.setString(2, c.circuitNm());
          ps.setInt(3, c.aAssetId());
          ps.setInt(4, c.zAssetId());
          ps.setInt(5, c.capMbps());
          ps.setInt(6, c.allocMbps());
          ps.setInt(7, c.roleCd());
          ps.setInt(8, c.statusCd());
        });
  }

  /** locations.cpp load_locations keeps every row in file order, duplicates included. */
  public void replaceLocations(List<Location> locations) {
    jdbc.update("DELETE FROM location");
    int[] seq = {0};
    jdbc.batchUpdate("INSERT INTO location (seq, loc_cd, cust_nm, loc_nm, market_cd, total_cap_mbps, "
        + "alloc_cap_mbps) VALUES (?,?,?,?,?,?,?)", locations, 500, (ps, l) -> {
          ps.setInt(1, seq[0]++);
          ps.setString(2, l.locCd());
          ps.setString(3, l.custNm());
          ps.setString(4, l.locNm());
          ps.setString(5, l.marketCd());
          ps.setInt(6, l.totalCapMbps());
          ps.setInt(7, l.allocCapMbps());
        });
  }

  /** Latest drop wins per (acct_id, period); a changed row is due for publishing again. */
  public void upsertUsage(List<Usage> usage, String source, Instant recordedAt) {
    int[] seq = {0};
    Timestamp at = Timestamp.from(recordedAt);
    jdbc.batchUpdate("INSERT INTO usage_record (acct_id, period, seq, usage_mb, source, recorded_at) "
        + "VALUES (?,?,?,?,?,?) ON CONFLICT (acct_id, period) DO UPDATE SET seq = EXCLUDED.seq, "
        + "usage_mb = EXCLUDED.usage_mb, source = EXCLUDED.source, recorded_at = EXCLUDED.recorded_at, "
        + "published_at = NULL", usage, 500, (ps, u) -> {
          ps.setString(1, u.acctId());
          ps.setString(2, u.period());
          ps.setInt(3, seq[0]++);
          ps.setLong(4, u.usageMb());
          ps.setString(5, source);
          ps.setTimestamp(6, at);
        });
  }

  public void markPublished(List<String[]> acctPeriods, Instant at) {
    Timestamp ts = Timestamp.from(at);
    jdbc.batchUpdate("UPDATE usage_record SET published_at = ? WHERE acct_id = ? AND period = ?",
        acctPeriods, 500, (ps, k) -> {
          ps.setTimestamp(1, ts);
          ps.setString(2, k[0]);
          ps.setString(3, k[1]);
        });
  }

  public long startRun(String dataDir, Instant startedAt) {
    return jdbc.queryForObject("INSERT INTO ingest_run (data_dir, started_at) VALUES (?, ?) RETURNING id",
        Long.class, dataDir, Timestamp.from(startedAt));
  }

  public void finishRun(long id, Instant finishedAt, int sites, int circuits, int locations, int usage,
      String report) {
    jdbc.update("UPDATE ingest_run SET finished_at = ?, sites_loaded = ?, circuits_loaded = ?, "
        + "locations_loaded = ?, usage_loaded = ?, report = ? WHERE id = ?",
        Timestamp.from(finishedAt), sites, circuits, locations, usage, report, id);
  }

  // ------------------------------------------------------------------ read API

  public List<Site> sites() {
    return jdbc.query("SELECT " + SITE_COLS + " FROM site ORDER BY asset_id", SITE);
  }

  public List<Site> sitesInRegion(String regionCd) {
    return jdbc.query("SELECT " + SITE_COLS + " FROM site WHERE region_cd = ? ORDER BY asset_id", SITE,
        regionCd);
  }

  public List<Site> sitesWithStatus(BigDecimal statusCd) {
    return jdbc.query("SELECT " + SITE_COLS + " FROM site WHERE status_cd = ? ORDER BY asset_id", SITE,
        statusCd);
  }

  /** SQLite orders TEXT by memcmp; COLLATE "C" gives the same order in PostgreSQL. */
  public List<Circuit> circuits() {
    return jdbc.query("SELECT " + CIRCUIT_COLS + " FROM circuit ORDER BY circuit_id COLLATE \"C\"", CIRCUIT);
  }

  public List<Circuit> circuit(String circuitId) {
    return jdbc.query("SELECT " + CIRCUIT_COLS + " FROM circuit WHERE circuit_id = ? "
        + "ORDER BY circuit_id COLLATE \"C\"", CIRCUIT, circuitId);
  }

  public List<Location> locations() {
    return jdbc.query("SELECT loc_cd, cust_nm, loc_nm, market_cd, total_cap_mbps, alloc_cap_mbps "
        + "FROM location ORDER BY seq", LOCATION);
  }

  public record UsageRow(String acctId, String period, long usageMb, String source, Instant recordedAt,
      Instant publishedAt) {
    static UsageRow map(ResultSet rs, int i) throws SQLException {
      Timestamp pub = rs.getTimestamp(6);
      return new UsageRow(rs.getString(1), rs.getString(2), rs.getLong(3), rs.getString(4),
          rs.getTimestamp(5).toInstant(), pub == null ? null : pub.toInstant());
    }
  }

  public List<UsageRow> usage() {
    return jdbc.query("SELECT acct_id, period, usage_mb, source, recorded_at, published_at FROM usage_record "
        + "ORDER BY seq", UsageRow::map);
  }
}
