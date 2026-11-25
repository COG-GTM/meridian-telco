package com.meridian.migration;

import java.io.IOException;
import java.math.BigDecimal;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.sql.Connection;
import java.sql.DriverManager;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.util.ArrayList;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.stream.Stream;
import org.flywaydb.core.Flyway;

/**
 * Phase 1 database decomposition. Splits the shared state into per-service PostgreSQL stores:
 * <ul>
 *   <li>meridian.db SITE, CIRCUIT → inventory (site, circuit)</li>
 *   <li>billing/accounts/*.rec (PROVINCE= records) → billing (account)</li>
 *   <li>network/devices.csv, reserved-ranges.conf, snmp-polling.conf → network</li>
 * </ul>
 * Each target is migrated with its owning service's Flyway scripts first, then reloaded and
 * verified (row counts and a SHA-256 over the copied rows, source vs target).
 *
 * <pre>
 * java -jar db-migration.jar --sqlite meridian.db --accounts billing/accounts --network network \
 *   --inventory-url jdbc:postgresql://.../inventory --billing-url ... --network-url ...
 * </pre>
 * Credentials: INVENTORY_DB_USER/PASSWORD, BILLING_DB_USER/PASSWORD, NETWORK_DB_USER/PASSWORD.
 */
public final class LegacyDataMigration {

  public record Target(String url, String user, String password) {
    Connection open() throws SQLException {
      return DriverManager.getConnection(url, user, password);
    }
  }

  public record Report(Map<String, Integer> rows, Map<String, Boolean> verified) {
    public boolean ok() {
      return verified.values().stream().allMatch(Boolean::booleanValue);
    }
  }

  private final Map<String, Integer> rows = new LinkedHashMap<>();
  private final Map<String, Boolean> verified = new LinkedHashMap<>();

  public static void main(String[] args) throws Exception {
    Map<String, String> a = new LinkedHashMap<>();
    for (int i = 0; i + 1 < args.length; i += 2) a.put(args[i], args[i + 1]);
    Report r = new LegacyDataMigration().run(
        Path.of(a.getOrDefault("--sqlite", "meridian.db")),
        Path.of(a.getOrDefault("--accounts", "billing/accounts")),
        Path.of(a.getOrDefault("--usage", "data/usage.csv")),
        Path.of(a.getOrDefault("--network", "network")),
        target(a, "--inventory-url", "INVENTORY"),
        target(a, "--billing-url", "BILLING"),
        target(a, "--network-url", "NETWORK"));
    r.rows().forEach((k, v) -> System.out.printf("  %-22s %6d rows  %s%n", k, v,
        r.verified().getOrDefault(k, true) ? "verified" : "MISMATCH"));
    System.exit(r.ok() ? 0 : 2);
  }

  private static Target target(Map<String, String> a, String flag, String env) {
    String url = a.get(flag);
    if (url == null) throw new IllegalArgumentException(flag + " is required");
    return new Target(url, System.getenv().getOrDefault(env + "_DB_USER", env.toLowerCase(Locale.ROOT)),
        System.getenv().getOrDefault(env + "_DB_PASSWORD", env.toLowerCase(Locale.ROOT)));
  }

  public Report run(Path sqlite, Path accountsDir, Path usageCsv, Path networkDir,
      Target inventory, Target billing, Target network) throws Exception {
    migrate(inventory, "inventory");
    migrate(billing, "billing");
    migrate(network, "network");
    try (Connection src = DriverManager.getConnection("jdbc:sqlite:" + sqlite.toAbsolutePath());
         Connection inv = inventory.open()) {
      copyTable(src, inv, "SITE", "site", List.of("ASSET_ID", "SITE_NM", "SITE_CD", "REGION_CD",
          "LAT", "LON", "STATUS_CD", "TOWER_REG", "TOTAL_CAP_MBPS", "ALLOC_CAP_MBPS"), "ASSET_ID");
      copyTable(src, inv, "CIRCUIT", "circuit", List.of("CIRCUIT_ID", "CIRCUIT_NM", "A_ASSET_ID",
          "Z_ASSET_ID", "CAP_MBPS", "ALLOC_MBPS", "ROLE_CD", "STATUS_CD"), "CIRCUIT_ID");
    }
    try (Connection bill = billing.open()) {
      loadAccounts(accountsDir, bill);
      loadUsage(usageCsv, bill);
    }
    try (Connection net = network.open()) {
      loadNetwork(networkDir, net);
    }
    return new Report(rows, verified);
  }

  private static void migrate(Target t, String service) {
    Flyway.configure().dataSource(t.url(), t.user(), t.password())
        .locations("classpath:db/" + service).load().migrate();
  }

  private void copyTable(Connection src, Connection dst, String srcTable, String dstTable,
      List<String> cols, String orderBy) throws SQLException {
    String colList = String.join(",", cols);
    List<Object[]> data = new ArrayList<>();
    try (Statement st = src.createStatement();
         ResultSet rs = st.executeQuery("SELECT " + colList + " FROM " + srcTable + " ORDER BY " + orderBy)) {
      while (rs.next()) {
        Object[] row = new Object[cols.size()];
        for (int i = 0; i < row.length; i++) row[i] = rs.getObject(i + 1);
        data.add(row);
      }
    }
    dst.setAutoCommit(false);
    try (Statement st = dst.createStatement()) {
      st.execute("TRUNCATE " + dstTable);
    }
    String marks = String.join(",", cols.stream().map(c -> "?").toList());
    try (PreparedStatement ps = dst.prepareStatement(
        "INSERT INTO " + dstTable + " (" + colList.toLowerCase(Locale.ROOT) + ") VALUES (" + marks + ")")) {
      for (Object[] row : data) {
        for (int i = 0; i < row.length; i++) ps.setObject(i + 1, row[i]);
        ps.addBatch();
      }
      ps.executeBatch();
    }
    dst.commit();
    dst.setAutoCommit(true);
    rows.put(dstTable, data.size());
    verified.put(dstTable, digest(src, "SELECT " + colList + " FROM " + srcTable + " ORDER BY " + orderBy)
        .equals(digest(dst, "SELECT " + colList.toLowerCase(Locale.ROOT) + " FROM " + dstTable + " ORDER BY " + orderBy.toLowerCase(Locale.ROOT))));
  }

  /** SHA-256 over every value rendered as text; numbers are normalized so 1 and 1.0 agree. */
  static String digest(Connection c, String sql) throws SQLException {
    try (Statement st = c.createStatement(); ResultSet rs = st.executeQuery(sql)) {
      MessageDigest md = MessageDigest.getInstance("SHA-256");
      int n = rs.getMetaData().getColumnCount();
      while (rs.next()) {
        for (int i = 1; i <= n; i++) {
          Object v = rs.getObject(i);
          String s = switch (v) {
            case null -> "\u0000";
            case Number num -> new BigDecimal(num.toString()).stripTrailingZeros().toPlainString();
            default -> v.toString();
          };
          md.update(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
          md.update((byte) 0x1f);
        }
        md.update((byte) 0x1e);
      }
      return HexFormat.of().formatHex(md.digest());
    } catch (NoSuchAlgorithmException e) {
      throw new IllegalStateException(e);
    }
  }

  // ---------------------------------------------------------------- billing account master

  static final List<String> ACCOUNT_KEYS = List.of("ACCT_ID", "BILLING_REF", "CUST_NM", "TAX_ID",
      "SVC_ADDR", "PROVINCE", "PLAN_CD", "PLAN_FEE", "INCLUDED_GB", "PREV_PLAN_FEE", "PLAN_CHG_DAY",
      "LINE_CNT", "PROMO_AMT", "PROMO_DT", "SUSP_START", "SUSP_END", "PRIOR_BAL", "PRIOR_DUE", "LOYALTY_PCT");

  /** billing/accounts.cpp parse_rec: KEY=VALUE lines, unknown keys ignored, numeric fields via atof/atoi. */
  static Map<String, String> parseRec(Path rec) throws IOException {
    Map<String, String> out = new LinkedHashMap<>();
    for (String line : Files.readAllLines(rec)) {
      int eq = line.indexOf('=');
      if (eq < 0) continue;
      out.put(line.substring(0, eq), line.substring(eq + 1));
    }
    return out;
  }

  private void loadAccounts(Path dir, Connection dst) throws SQLException, IOException {
    List<Path> recs;
    try (Stream<Path> s = Files.list(dir)) {
      recs = s.filter(p -> p.getFileName().toString().endsWith(".rec")
          && p.getFileName().toString().length() >= 5).sorted().toList();
    }
    dst.setAutoCommit(false);
    try (Statement st = dst.createStatement()) {
      st.execute("TRUNCATE account");
    }
    try (PreparedStatement ps = dst.prepareStatement("INSERT INTO account (acct_id, billing_ref, cust_nm, "
        + "tax_id, svc_addr, province, plan_cd, plan_fee, included_gb, prev_plan_fee, plan_chg_day, line_cnt, "
        + "promo_amt, promo_dt, susp_start, susp_end, prior_bal, prior_due, loyalty_pct, source_file) "
        + "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)")) {
      for (Path rec : recs) {
        Map<String, String> r = parseRec(rec);
        int i = 1;
        ps.setString(i++, r.getOrDefault("ACCT_ID", ""));
        for (String k : List.of("BILLING_REF", "CUST_NM", "TAX_ID", "SVC_ADDR", "PROVINCE", "PLAN_CD")) {
          ps.setString(i++, r.getOrDefault(k, ""));
        }
        ps.setBigDecimal(i++, Atof.decimal(r.get("PLAN_FEE"), "0"));
        ps.setLong(i++, Atof.integer(r.get("INCLUDED_GB"), 0));
        ps.setBigDecimal(i++, Atof.decimal(r.get("PREV_PLAN_FEE"), "0"));
        ps.setInt(i++, (int) Atof.integer(r.get("PLAN_CHG_DAY"), 0));
        ps.setInt(i++, (int) Atof.integer(r.get("LINE_CNT"), 1));
        ps.setBigDecimal(i++, Atof.decimal(r.get("PROMO_AMT"), "0"));
        ps.setString(i++, r.getOrDefault("PROMO_DT", ""));
        ps.setInt(i++, (int) Atof.integer(r.get("SUSP_START"), 0));
        ps.setInt(i++, (int) Atof.integer(r.get("SUSP_END"), 0));
        ps.setBigDecimal(i++, Atof.decimal(r.get("PRIOR_BAL"), "0"));
        ps.setString(i++, r.getOrDefault("PRIOR_DUE", ""));
        ps.setBigDecimal(i++, Atof.decimal(r.get("LOYALTY_PCT"), "0"));
        ps.setString(i, rec.getFileName().toString());
        ps.addBatch();
      }
      ps.executeBatch();
    }
    dst.commit();
    dst.setAutoCommit(true);
    rows.put("account", recs.size());
    try (Statement st = dst.createStatement();
         ResultSet rs = st.executeQuery("SELECT count(*), count(DISTINCT acct_id) FROM account")) {
      rs.next();
      verified.put("account", rs.getInt(1) == recs.size() && rs.getInt(2) == recs.size());
    }
  }

  /**
   * data/usage.csv into billing's usage_record, so billing has the period's usage before the Kafka feed runs.
   * Like the SeedLoader, a row already delivered by the feed is never overwritten.
   */
  private void loadUsage(Path csv, Connection dst) throws SQLException, IOException {
    if (!Files.isRegularFile(csv)) return;
    Map<String, Long> want = new LinkedHashMap<>();
    for (Map<String, String> r : readCsv(csv)) {
      want.putIfAbsent(r.get("ACCT_ID") + "|" + r.get("PERIOD"), Atof.integer(r.get("USAGE_MB"), 0));
    }
    try (PreparedStatement ps = dst.prepareStatement("INSERT INTO usage_record (acct_id, period, seq, usage_mb, "
        + "source) VALUES (?, ?, nextval('usage_record_seq'), ?, ?) ON CONFLICT (acct_id, period) DO NOTHING")) {
      for (var e : want.entrySet()) {
        String[] k = e.getKey().split("\\|", 2);
        ps.setString(1, k[0]);
        ps.setString(2, k[1]);
        ps.setLong(3, e.getValue());
        ps.setString(4, "migration:" + csv.getFileName());
        ps.addBatch();
      }
      ps.executeBatch();
    }
    int present = 0;
    try (Statement st = dst.createStatement();
         ResultSet rs = st.executeQuery("SELECT acct_id, period FROM usage_record")) {
      while (rs.next()) if (want.containsKey(rs.getString(1) + "|" + rs.getString(2))) present++;
    }
    rows.put("usage_record", want.size());
    verified.put("usage_record", present == want.size());
  }

  /** mediation/csv.h read_csv: header row, quotes toggle and are dropped, empty lines skipped, missing fields "". */
  static List<Map<String, String>> readCsv(Path path) throws IOException {
    List<String> lines = Files.readAllLines(path);
    List<Map<String, String>> out = new ArrayList<>();
    if (lines.isEmpty()) return out;
    List<String> hdr = splitLine(lines.get(0));
    for (String line : lines.subList(1, lines.size())) {
      if (line.isEmpty()) continue;
      List<String> v = splitLine(line);
      Map<String, String> row = new LinkedHashMap<>();
      for (String h : hdr) row.putIfAbsent(h, "");
      for (int i = 0; i < hdr.size() && i < v.size(); i++) row.put(hdr.get(i), v.get(i));
      out.add(row);
    }
    return out;
  }

  static List<String> splitLine(String line) {
    List<String> out = new ArrayList<>();
    StringBuilder cur = new StringBuilder();
    boolean q = false;
    for (char c : line.toCharArray()) {
      if (c == '"') q = !q;
      else if (c == ',' && !q) { out.add(cur.toString()); cur.setLength(0); }
      else cur.append(c);
    }
    out.add(cur.toString());
    return out;
  }

  // ---------------------------------------------------------------- network config files

  private void loadNetwork(Path dir, Connection dst) throws SQLException, IOException {
    dst.setAutoCommit(false);
    try (Statement st = dst.createStatement()) {
      st.execute("TRUNCATE device, reserved_range, snmp_target");
    }
    int n = 0;
    try (PreparedStatement ps = dst.prepareStatement("INSERT INTO device (seq, device_name, region, role, "
        + "mgmt_ip, site_cd, external_bgp) VALUES (?,?,?,?,?,?,?)")) {
      for (Map<String, String> v : readCsv(dir.resolve("devices.csv"))) {
        ps.setInt(1, ++n);
        ps.setString(2, v.getOrDefault("device_name", ""));
        ps.setString(3, v.getOrDefault("region", ""));
        ps.setString(4, v.getOrDefault("role", ""));
        ps.setString(5, v.getOrDefault("mgmt_ip", ""));
        ps.setString(6, v.getOrDefault("site_cd", ""));
        ps.setBoolean(7, "1".equals(v.get("external_bgp")));
        ps.addBatch();
      }
      ps.executeBatch();
    }
    rows.put("device", n);

    int reserved = 0;
    try (PreparedStatement ps = dst.prepareStatement("INSERT INTO reserved_range (cidr, purpose) VALUES (?,?)")) {
      for (String line : Files.readAllLines(dir.resolve("reserved-ranges.conf"))) {
        String[] w = line.strip().split("\\s+");
        if (w.length >= 2 && w[0].equals("reserve")) {
          ps.setString(1, w[1]);
          ps.setString(2, w.length >= 4 && w[2].equals("purpose") ? w[3] : null);
          ps.addBatch();
          reserved++;
        }
      }
      ps.executeBatch();
    }
    rows.put("reserved_range", reserved);

    int targets = 0;
    try (PreparedStatement ps = dst.prepareStatement(
        "INSERT INTO snmp_target (position, ip, interval_seconds, host) VALUES (?,?,?,?)")) {
      for (String line : Files.readAllLines(dir.resolve("snmp-polling.conf"))) {
        String[] w = line.strip().split("\\s+");
        if (w.length >= 2 && w[0].equals("target")) {
          Map<String, String> kv = new LinkedHashMap<>();
          for (int i = 2; i + 1 < w.length; i += 2) kv.put(w[i], w[i + 1]);
          ps.setInt(1, targets++);
          ps.setString(2, w[1]);
          ps.setObject(3, kv.containsKey("interval") ? Integer.valueOf(kv.get("interval")) : null);
          ps.setString(4, kv.get("host"));
          ps.addBatch();
        }
      }
      ps.executeBatch();
    }
    rows.put("snmp_target", targets);
    dst.commit();
    dst.setAutoCommit(true);
  }
}
