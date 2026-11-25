package com.meridian.migration;

import static org.assertj.core.api.Assertions.assertThat;

import java.nio.file.Files;
import java.nio.file.Path;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.ResultSet;
import java.sql.Statement;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;
import org.testcontainers.utility.DockerImageName;

/** Splits a meridian.db built from data/ plus the flat-file masters into three PostgreSQL stores. */
@Testcontainers(disabledWithoutDocker = true)
class LegacyDataMigrationTest {

  static final DockerImageName PG = DockerImageName.parse(
      System.getProperty("meridian.postgres.image", "postgres:16-alpine")).asCompatibleSubstituteFor("postgres");

  @Container static PostgreSQLContainer<?> inventory = new PostgreSQLContainer<>(PG).withDatabaseName("inventory");
  @Container static PostgreSQLContainer<?> billing = new PostgreSQLContainer<>(PG).withDatabaseName("billing");
  @Container static PostgreSQLContainer<?> network = new PostgreSQLContainer<>(PG).withDatabaseName("network");

  @TempDir Path tmp;

  static Path root() {
    return Path.of(System.getProperty("meridian.root", "../.."));
  }

  /** Same schema and rows as mediation/store.cpp produces from data/sites.csv and data/circuits.csv. */
  Path legacyStore() throws Exception {
    Path db = tmp.resolve("meridian.db");
    try (Connection c = DriverManager.getConnection("jdbc:sqlite:" + db); Statement st = c.createStatement()) {
      st.execute("CREATE TABLE SITE(ASSET_ID INTEGER PRIMARY KEY, SITE_NM TEXT, SITE_CD TEXT, REGION_CD TEXT,"
          + " LAT REAL, LON REAL, STATUS_CD INTEGER, TOWER_REG TEXT, TOTAL_CAP_MBPS INTEGER, ALLOC_CAP_MBPS INTEGER)");
      st.execute("CREATE TABLE CIRCUIT(CIRCUIT_ID TEXT PRIMARY KEY, CIRCUIT_NM TEXT, A_ASSET_ID INTEGER,"
          + " Z_ASSET_ID INTEGER, CAP_MBPS INTEGER, ALLOC_MBPS INTEGER, ROLE_CD INTEGER, STATUS_CD INTEGER)");
      load(c, "SITE", root().resolve("data/sites.csv"), 10);
      load(c, "CIRCUIT", root().resolve("data/circuits.csv"), 8);
    }
    return db;
  }

  static void load(Connection c, String table, Path csv, int cols) throws Exception {
    var lines = Files.readAllLines(csv);
    var marks = String.join(",", java.util.Collections.nCopies(cols, "?"));
    try (var ps = c.prepareStatement("INSERT OR REPLACE INTO " + table + " VALUES(" + marks + ")")) {
      for (String line : lines.subList(1, lines.size())) {
        String[] v = line.split(",", -1);
        for (int i = 0; i < cols; i++) ps.setString(i + 1, v[i]);
        ps.executeUpdate();
      }
    }
  }

  static LegacyDataMigration.Target t(PostgreSQLContainer<?> pg) {
    return new LegacyDataMigration.Target(pg.getJdbcUrl(), pg.getUsername(), pg.getPassword());
  }

  @Test
  void splitsSharedStateIntoPerServiceStores() throws Exception {
    var report = new LegacyDataMigration().run(legacyStore(), root().resolve("billing/accounts"),
        root().resolve("network"), t(inventory), t(billing), t(network));

    assertThat(report.ok()).as(report.toString()).isTrue();
    assertThat(report.rows()).containsEntry("site", 412).containsEntry("circuit", 240)
        .containsEntry("account", 200).containsEntry("device", 147)
        .containsEntry("reserved_range", 2).containsEntry("snmp_target", 138);

    try (Connection c = DriverManager.getConnection(billing.getJdbcUrl(), billing.getUsername(), billing.getPassword());
         Statement st = c.createStatement();
         ResultSet rs = st.executeQuery("SELECT province, plan_fee, prior_bal, line_cnt FROM account WHERE acct_id='BEACON-004417'")) {
      assertThat(rs.next()).isTrue();
      assertThat(rs.getString(1)).isEqualTo("BC");
      assertThat(rs.getBigDecimal(2)).isEqualByComparingTo("4200.0");
      assertThat(rs.getBigDecimal(3)).isEqualByComparingTo("700.9");
      assertThat(rs.getInt(4)).isEqualTo(14);
    }
  }

  @Test
  void rerunIsIdempotent() throws Exception {
    Path db = legacyStore();
    var m = new LegacyDataMigration();
    m.run(db, root().resolve("billing/accounts"), root().resolve("network"), t(inventory), t(billing), t(network));
    var again = new LegacyDataMigration().run(db, root().resolve("billing/accounts"), root().resolve("network"),
        t(inventory), t(billing), t(network));
    assertThat(again.ok()).isTrue();
    assertThat(again.rows()).containsEntry("site", 412).containsEntry("account", 200);
  }
}
