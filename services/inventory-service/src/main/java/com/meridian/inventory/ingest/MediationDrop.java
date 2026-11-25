package com.meridian.inventory.ingest;

import java.math.BigDecimal;
import java.math.RoundingMode;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * One telemetry drop (data/): the raw CSV rows of sites.csv, circuits.csv, locations.csv and
 * usage.csv, read concurrently on virtual threads, plus their typed form as mediation/store.cpp,
 * mediation/locations.cpp and billing/accounts.cpp load_usage convert them.
 */
public record MediationDrop(Path dataDir, List<Map<String, String>> siteRows,
    List<Map<String, String>> circuitRows, List<Map<String, String>> locationRows,
    List<Map<String, String>> usageRows) {

  public record Site(int assetId, String siteNm, String siteCd, String regionCd, double lat,
      double lon, int statusCd, String towerReg, int totalCapMbps, int allocCapMbps) {}

  public record Circuit(String circuitId, String circuitNm, int aAssetId, int zAssetId,
      int capMbps, int allocMbps, int roleCd, int statusCd) {}

  public record Location(String locCd, String custNm, String locNm, String marketCd,
      int totalCapMbps, int allocCapMbps) {}

  public record Usage(String acctId, String period, long usageMb) {}

  public static MediationDrop load(Path dataDir) {
    try (ExecutorService pool = Executors.newVirtualThreadPerTaskExecutor()) {
      Future<List<Map<String, String>>> sites = pool.submit(() -> TelemetryCsv.read(dataDir.resolve("sites.csv")));
      Future<List<Map<String, String>>> circuits = pool.submit(() -> TelemetryCsv.read(dataDir.resolve("circuits.csv")));
      Future<List<Map<String, String>>> locations = pool.submit(() -> TelemetryCsv.read(dataDir.resolve("locations.csv")));
      Future<List<Map<String, String>>> usage = pool.submit(() -> TelemetryCsv.read(dataDir.resolve("usage.csv")));
      return new MediationDrop(dataDir, sites.get(), circuits.get(), locations.get(), usage.get());
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      throw new IllegalStateException("interrupted reading " + dataDir, e);
    } catch (ExecutionException e) {
      throw new IllegalStateException("cannot read drop " + dataDir, e.getCause());
    }
  }

  public List<Site> sites() {
    return siteRows.stream().map(r -> new Site(
        TelemetryCsv.toInt(f(r, "ASSET_ID")), f(r, "SITE_NM"), f(r, "SITE_CD"), f(r, "REGION_CD"),
        storedReal(f(r, "LAT")), storedReal(f(r, "LON")), TelemetryCsv.toInt(f(r, "STATUS_CD")),
        f(r, "TOWER_REG"), TelemetryCsv.toInt(f(r, "TOTAL_CAP_MBPS")),
        TelemetryCsv.toInt(f(r, "ALLOC_CAP_MBPS")))).toList();
  }

  public List<Circuit> circuits() {
    return circuitRows.stream().map(r -> new Circuit(
        f(r, "CIRCUIT_ID"), f(r, "CIRCUIT_NM"), TelemetryCsv.toInt(f(r, "A_ASSET_ID")),
        TelemetryCsv.toInt(f(r, "Z_ASSET_ID")), TelemetryCsv.toInt(f(r, "CAP_MBPS")),
        TelemetryCsv.toInt(f(r, "ALLOC_MBPS")), TelemetryCsv.toInt(f(r, "ROLE_CD")),
        TelemetryCsv.toInt(f(r, "STATUS_CD")))).toList();
  }

  public List<Location> locations() {
    return locationRows.stream().map(r -> new Location(
        f(r, "LOC_CD"), f(r, "CUST_NM"), f(r, "LOC_NM"), f(r, "MARKET_CD"),
        TelemetryCsv.toInt(f(r, "TOTAL_CAP_MBPS")), TelemetryCsv.toInt(f(r, "ALLOC_CAP_MBPS")))).toList();
  }

  public List<Usage> usage() {
    return usageRows.stream().map(r -> new Usage(
        f(r, "ACCT_ID"), f(r, "PERIOD"), TelemetryCsv.toLong(f(r, "USAGE_MB")))).toList();
  }

  /** std::map operator[]: a missing field reads as "". */
  private static String f(Map<String, String> r, String key) {
    return r.getOrDefault(key, "");
  }

  /** store.cpp writes LAT/LON with printf %f, so SQLite holds the value rounded to 6 decimals. */
  static double storedReal(String raw) {
    double d = TelemetryCsv.toDouble(raw);
    if (Double.isNaN(d) || Double.isInfinite(d)) return d;
    return new BigDecimal(d).setScale(6, RoundingMode.HALF_EVEN).doubleValue();
  }
}
