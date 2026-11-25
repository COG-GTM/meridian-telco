package com.meridian.network.ipam;

import java.io.IOException;
import java.io.Reader;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVRecord;
import org.apache.commons.csv.DuplicateHeaderMode;

/** Parsers for the network/ config files: devices.csv, reserved-ranges.conf, snmp-polling.conf. */
public final class NetworkFiles {

  public static final String DEVICES = "devices.csv";
  public static final String RESERVED_RANGES = "reserved-ranges.conf";
  public static final String SNMP_POLLING = "snmp-polling.conf";

  private static final CSVFormat CSV = CSVFormat.DEFAULT.builder()
      .setHeader()
      .setSkipHeaderRecord(true)
      .setIgnoreEmptyLines(true)
      .setAllowMissingColumnNames(true)
      .setDuplicateHeaderMode(DuplicateHeaderMode.ALLOW_ALL)
      .build();

  private NetworkFiles() {}

  /** devices.csv rows in file order; a missing file reads as no rows (like mediation/csv.h). */
  public static List<Device> readDevices(Path csv) {
    List<Device> out = new ArrayList<>();
    try (Reader in = Files.newBufferedReader(csv, StandardCharsets.UTF_8)) {
      int seq = 0;
      for (CSVRecord r : CSV.parse(in)) {
        out.add(new Device(++seq, col(r, "device_name"), col(r, "region"), col(r, "role"),
            col(r, "mgmt_ip"), col(r, "site_cd"), "1".equals(col(r, "external_bgp"))));
      }
    } catch (NoSuchFileException e) {
      return List.of();
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    return out;
  }

  public static List<ReservedRange> readReservedRanges(Path conf) {
    List<ReservedRange> out = new ArrayList<>();
    for (String[] w : directives(conf)) {
      if (w[0].equals("reserve") && w.length >= 2) {
        out.add(new ReservedRange(w[1], options(w).get("purpose")));
      }
    }
    return out;
  }

  /** Poller targets in positional order. The {@code community} line is deliberately not read. */
  public static List<SnmpTarget> readSnmpTargets(Path conf) {
    List<SnmpTarget> out = new ArrayList<>();
    for (String[] w : directives(conf)) {
      if (w[0].equals("target") && w.length >= 2) {
        Map<String, String> o = options(w);
        out.add(new SnmpTarget(out.size(), w[1], parseInterval(o.get("interval")), o.get("host")));
      }
    }
    return out;
  }

  private static String col(CSVRecord r, String name) {
    return r.isSet(name) ? r.get(name) : "";
  }

  private static Integer parseInterval(String s) {
    if (s == null) return null;
    try {
      return Integer.valueOf(s);
    } catch (NumberFormatException e) {
      return null;
    }
  }

  /** Non-comment lines split into words; trailing "key value" pairs after the argument. */
  private static List<String[]> directives(Path conf) {
    List<String> lines;
    try {
      lines = Files.readAllLines(conf, StandardCharsets.UTF_8);
    } catch (NoSuchFileException e) {
      return List.of();
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    List<String[]> out = new ArrayList<>();
    for (String line : lines) {
      String s = line.strip();
      if (s.isEmpty() || s.startsWith("#")) continue;
      out.add(s.split("\\s+"));
    }
    return out;
  }

  private static Map<String, String> options(String[] w) {
    Map<String, String> kv = new HashMap<>();
    for (int i = 2; i + 1 < w.length; i += 2) kv.put(w[i], w[i + 1]);
    return kv;
  }
}
