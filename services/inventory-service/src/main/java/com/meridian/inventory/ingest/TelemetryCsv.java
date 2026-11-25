package com.meridian.inventory.ingest;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVParser;
import org.apache.commons.csv.CSVRecord;

/**
 * Reads a telemetry drop (data/*.csv) the way mediation/csv.h does: the header row names the
 * fields, each row becomes field name to raw text.
 *
 * <p>Fields are split by Apache Commons CSV. A thin compatibility layer keeps the csv.h quirks
 * that downstream output depends on:
 * <ul>
 *   <li>records are framed by {@code getline}: one line is one record, a quoted field never spans
 *       lines, only an empty line is skipped and a trailing {@code \r} stays in the last field;</li>
 *   <li>a row shorter than the header omits the missing fields, extra columns are dropped, a
 *       repeated header name keeps the last value;</li>
 *   <li>no trimming; a line where csv.h and RFC 4180 disagree ({@code \r}, doubled, mid-field or
 *       unbalanced quotes) is split with the csv.h quote-toggle rule instead.</li>
 * </ul>
 */
public final class TelemetryCsv {

  private static final CSVFormat FORMAT = CSVFormat.RFC4180.builder()
      .setIgnoreSurroundingSpaces(false)
      .setTrim(false)
      .setIgnoreEmptyLines(false)
      .build();

  private static final Pattern ATOL = Pattern.compile("^[ \\t\\n\\u000B\\f\\r]*([+-]?\\d+)");
  private static final Pattern ATOF =
      Pattern.compile("^[ \\t\\n\\u000B\\f\\r]*([+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?)");
  private static final BigInteger LONG_MIN = BigInteger.valueOf(Long.MIN_VALUE);
  private static final BigInteger LONG_MAX = BigInteger.valueOf(Long.MAX_VALUE);

  private TelemetryCsv() {}

  public static List<Map<String, String>> read(Path csv) {
    String text;
    try {
      text = Files.readString(csv, StandardCharsets.UTF_8);
    } catch (NoSuchFileException e) {
      return List.of();
    } catch (IOException e) {
      if (!Files.isReadable(csv)) return List.of();
      throw new UncheckedIOException(e);
    }
    List<String> lines = getlines(text);
    List<Map<String, String>> rows = new ArrayList<>();
    if (lines.isEmpty()) return rows;
    List<String> hdr = splitLine(lines.get(0));
    for (String line : lines.subList(1, lines.size())) {
      if (line.isEmpty()) continue;
      List<String> v = splitLine(line);
      Map<String, String> r = new LinkedHashMap<>();
      for (int i = 0; i < hdr.size() && i < v.size(); i++) r.put(hdr.get(i), v.get(i));
      rows.add(r);
    }
    return rows;
  }

  /** atoi semantics of csv.h to_int: leading whitespace and sign, digits up to the first non-digit, else 0. */
  public static int toInt(String s) {
    return (int) toLong(s);
  }

  /** glibc atol/strtol: longest integer prefix, clamped to the long range. */
  public static long toLong(String s) {
    if (s == null || s.isEmpty()) return 0;
    Matcher m = ATOL.matcher(s);
    if (!m.find()) return 0;
    BigInteger v = new BigInteger(m.group(1));
    return v.max(LONG_MIN).min(LONG_MAX).longValue();
  }

  /** csv.h to_dbl (atof) for decimal input: longest decimal prefix, else 0. */
  public static double toDouble(String s) {
    if (s == null || s.isEmpty()) return 0.0;
    Matcher m = ATOF.matcher(s);
    return m.find() ? Double.parseDouble(m.group(1)) : 0.0;
  }

  /** std::getline over the whole file: split on '\n' only, no empty record after a final '\n'. */
  static List<String> getlines(String text) {
    List<String> out = new ArrayList<>();
    int from = 0;
    while (from < text.length()) {
      int nl = text.indexOf('\n', from);
      if (nl < 0) {
        out.add(text.substring(from));
        break;
      }
      out.add(text.substring(from, nl));
      from = nl + 1;
    }
    return out;
  }

  static List<String> splitLine(String line) {
    List<String> fields = commonsSplit(line);
    return fields != null ? fields : legacySplit(line);
  }

  /**
   * Commons CSV split, or null when its RFC 4180 reading may differ from csv.h: any '"' (csv.h
   * toggles on every quote and keeps text around closing quotes, Commons skips or rejects it) or
   * '\r' (Commons treats it as a record separator, csv.h keeps it in the field).
   */
  private static List<String> commonsSplit(String line) {
    if (line.indexOf('\r') >= 0 || line.indexOf('"') >= 0) return null;
    try (CSVParser p = CSVParser.parse(line, FORMAT)) {
      List<CSVRecord> records = p.getRecords();
      return records.size() == 1 ? records.get(0).toList() : null;
    } catch (IOException | UncheckedIOException | IllegalStateException e) {
      return null;
    }
  }

  /** csv.h split_line: every '"' toggles quoting and is dropped; ',' outside quotes separates. */
  static List<String> legacySplit(String line) {
    List<String> out = new ArrayList<>();
    StringBuilder cur = new StringBuilder();
    boolean q = false;
    for (int i = 0; i < line.length(); i++) {
      char c = line.charAt(i);
      if (c == '"') {
        q = !q;
        continue;
      }
      if (c == ',' && !q) {
        out.add(cur.toString());
        cur.setLength(0);
        continue;
      }
      cur.append(c);
    }
    out.add(cur.toString());
    return out;
  }
}
