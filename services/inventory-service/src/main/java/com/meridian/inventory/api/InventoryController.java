package com.meridian.inventory.api;

import com.meridian.inventory.capacity.CapacityRule;
import com.meridian.inventory.ingest.MediationDrop.Circuit;
import com.meridian.inventory.ingest.MediationDrop.Location;
import com.meridian.inventory.ingest.MediationDrop.Site;
import com.meridian.inventory.ingest.TelemetryCsv;
import com.meridian.inventory.status.CircuitCounter;
import com.meridian.inventory.status.SiteStatus;
import com.meridian.inventory.store.InventoryStore;
import jakarta.servlet.http.HttpServletRequest;
import java.math.BigDecimal;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * The :8081 read API (inventory-api/main.cpp). Field names, key order, number formatting and the
 * prefix routing of httpd.h (any path starting with /sites, /circuits or /capacity) are part of
 * the frozen contract. Query values are taken raw from the query string, as legacy did, and bound
 * as SQL parameters.
 */
@RestController
public class InventoryController {

  /** Signed SQLite numeric literals (decimal, exponent, hex); possessive so matching is linear. */
  private static final Pattern DECIMAL = Pattern.compile("[+-]?+(?:\\d++(?:\\.\\d*+)?+|\\.\\d++)(?:[eE][+-]?+\\d++)?+");
  private static final Pattern HEX = Pattern.compile("([+-]?+)0[xX]([0-9a-fA-F]++)");

  private final InventoryStore store;
  private final CapacityRule capacity;

  public InventoryController(InventoryStore store, CapacityRule capacity) {
    this.store = store;
    this.capacity = capacity;
  }

  @RequestMapping({"/sites", "/sites*", "/sites*/**"})
  public ResponseEntity<String> sites(HttpServletRequest req) {
    Map<String, String> q = LegacyQuery.parse(req.getQueryString());
    List<Site> rows;
    if (q.containsKey("REGION_CD")) {
      rows = store.sitesInRegion(q.get("REGION_CD"));
    } else if (q.containsKey("STATUS_CD")) {
      // legacy spliced the value into the SQL; anything that is not a number failed the query
      Optional<BigDecimal> status = sqliteNumber(q.get("STATUS_CD"));
      rows = status.isPresent() ? store.sitesWithStatus(status.get()) : List.of();
    } else {
      rows = store.sites();
    }
    StringBuilder out = new StringBuilder("{\"count\":").append(rows.size()).append(",\"sites\":[");
    for (int i = 0; i < rows.size(); i++) {
      if (i > 0) out.append(',');
      Site s = rows.get(i);
      out.append("{\"ASSET_ID\":").append(s.assetId())
          .append(",\"SITE_NM\":\"").append(LegacyJson.escape(s.siteNm()))
          .append("\",\"SITE_CD\":\"").append(LegacyJson.raw(s.siteCd()))
          .append("\",\"REGION_CD\":\"").append(LegacyJson.raw(s.regionCd()))
          .append("\",\"LAT\":").append(LegacyJson.sqliteReal(s.lat()))
          .append(",\"LON\":").append(LegacyJson.sqliteReal(s.lon()))
          .append(",\"STATUS_CD\":").append(s.statusCd())
          .append(",\"STATUS_LABEL\":\"").append(SiteStatus.label(s.statusCd()))
          .append("\",\"TOWER_REG\":\"").append(LegacyJson.raw(s.towerReg()))
          .append("\",\"TOTAL_CAP_MBPS\":").append(s.totalCapMbps())
          .append(",\"ALLOC_CAP_MBPS\":").append(s.allocCapMbps())
          .append(",\"AVAIL_CAP_MBPS\":").append(capacity.available(s.totalCapMbps(), s.allocCapMbps()))
          .append('}');
    }
    return json(HttpStatus.OK, out.append("]}").toString());
  }

  @RequestMapping({"/circuits", "/circuits*", "/circuits*/**"})
  public ResponseEntity<String> circuits(HttpServletRequest req) {
    Map<String, String> q = LegacyQuery.parse(req.getQueryString());
    List<Circuit> rows = q.containsKey("CIRCUIT_ID") ? store.circuit(q.get("CIRCUIT_ID")) : store.circuits();
    StringBuilder out = new StringBuilder("{\"count\":").append(rows.size())
        .append(",\"active_count\":").append(CircuitCounter.countActive(rows, Circuit::statusCd))
        .append(",\"circuits\":[");
    for (int i = 0; i < rows.size(); i++) {
      if (i > 0) out.append(',');
      Circuit c = rows.get(i);
      out.append("{\"CIRCUIT_ID\":\"").append(LegacyJson.raw(c.circuitId()))
          .append("\",\"CIRCUIT_NM\":\"").append(LegacyJson.escape(c.circuitNm()))
          .append("\",\"A_ASSET_ID\":").append(c.aAssetId())
          .append(",\"Z_ASSET_ID\":").append(c.zAssetId())
          .append(",\"CAP_MBPS\":").append(c.capMbps())
          .append(",\"ALLOC_MBPS\":").append(c.allocMbps())
          .append(",\"ROLE_CD\":").append(c.roleCd())
          .append(",\"STATUS_CD\":").append(c.statusCd())
          .append(",\"AVAIL_MBPS\":").append(capacity.available(c.capMbps(), c.allocMbps()))
          .append('}');
    }
    return json(HttpStatus.OK, out.append("]}").toString());
  }

  /** Sales capacity check: one row per serviceable location; requested= is the quoted mbps. */
  @RequestMapping({"/capacity", "/capacity*", "/capacity*/**"})
  public ResponseEntity<String> capacity(HttpServletRequest req) {
    Map<String, String> q = LegacyQuery.parse(req.getQueryString());
    int requested = q.containsKey("requested") ? TelemetryCsv.toInt(q.get("requested")) : 0;
    String market = q.getOrDefault("market", "");
    StringBuilder out = new StringBuilder("{\"rule\":\"AVAIL_CAP_MBPS = TOTAL_CAP_MBPS - ALLOC_CAP_MBPS\",")
        .append("\"requested_mbps\":").append(requested).append(",\"locations\":[");
    int emitted = 0;
    for (Location l : store.locations()) {
      if (!market.isEmpty() && !market.equals(l.marketCd())) continue;
      if (emitted++ > 0) out.append(',');
      out.append("{\"LOC_CD\":\"").append(LegacyJson.raw(l.locCd()))
          .append("\",\"CUST_NM\":\"").append(LegacyJson.escape(l.custNm()))
          .append("\",\"LOC_NM\":\"").append(LegacyJson.escape(l.locNm()))
          .append("\",\"MARKET_CD\":\"").append(LegacyJson.raw(l.marketCd()))
          .append("\",\"TOTAL_CAP_MBPS\":").append(l.totalCapMbps())
          .append(",\"ALLOC_CAP_MBPS\":").append(l.allocCapMbps())
          .append(",\"AVAIL_CAP_MBPS\":").append(capacity.available(l.totalCapMbps(), l.allocCapMbps()))
          .append(",\"UTILIZATION_PCT\":").append(capacity.utilizationPct(l.totalCapMbps(), l.allocCapMbps()))
          .append(",\"CAN_SUPPORT\":")
          .append(capacity.canSupport(l.totalCapMbps(), l.allocCapMbps(), requested))
          .append('}');
    }
    return json(HttpStatus.OK, out.append("]}").toString());
  }

  /** The value of STATUS_CD=&lt;v&gt; as SQLite evaluated it, if it was a numeric literal. */
  static Optional<BigDecimal> sqliteNumber(String v) {
    Matcher hex = HEX.matcher(v);
    if (hex.matches()) {
      String digits = hex.group(2).replaceFirst("^0++", "");
      if (digits.length() > 16) return Optional.empty(); // SQLite: hex literal too big
      long n = digits.isEmpty() ? 0 : Long.parseUnsignedLong(digits, 16);
      return Optional.of(BigDecimal.valueOf("-".equals(hex.group(1)) ? -n : n));
    }
    if (!DECIMAL.matcher(v).matches()) return Optional.empty();
    try {
      return Optional.of(new BigDecimal(v));
    } catch (NumberFormatException | ArithmeticException e) {
      return Optional.empty(); // exponent out of range: SQLite reads it as +-Inf, no match
    }
  }

  /** Every other path: httpd.h answers 404 {"error":"not found"}. */
  @RequestMapping("/**")
  public ResponseEntity<String> notFound() {
    return json(HttpStatus.NOT_FOUND, "{\"error\":\"not found\"}");
  }

  static ResponseEntity<String> json(HttpStatus status, String body) {
    return ResponseEntity.status(status).contentType(MediaType.APPLICATION_JSON).body(body);
  }
}
