package com.meridian.inventory.ingest;

import static org.assertj.core.api.Assertions.assertThat;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/**
 * More csv.h behaviour where RFC 4180 (Commons CSV) would differ. Expected values observed by
 * running mediation/csv.h read_csv/to_int on the same bytes (g++ 11 probe, 2026-11-25).
 */
class TelemetryCsvLegacyQuirksTest {

  @TempDir Path dir;

  List<Map<String, String>> read(String content) throws IOException {
    Path f = dir.resolve("drop.csv");
    Files.writeString(f, content);
    return TelemetryCsv.read(f);
  }

  @Test
  void crlfKeepsTheCarriageReturnAndABlankCrlfLineIsARow() throws IOException {
    List<Map<String, String>> rows = read("A,B,C\r\n1,2,3\r\n\r\n");
    assertThat(rows).hasSize(2);
    // the header's last name is "C\r", so the row's last field is filed under it
    assertThat(rows.get(0)).containsEntry("A", "1").containsEntry("B", "2").containsEntry("C\r", "3\r");
    assertThat(rows.get(1)).containsExactlyEntriesOf(Map.of("A", "\r"));
  }

  @Test
  void everyQuoteTogglesAndIsDropped() throws IOException {
    Map<String, String> r = read("A,B,C\n\"a\"\"b\",x\"y,z\"w,q\n").get(0);
    assertThat(r).containsEntry("A", "ab").containsEntry("B", "xy,zw").containsEntry("C", "q");
  }

  @Test
  void unbalancedQuoteRunsToEndOfLineAndNeverSpansLines() throws IOException {
    List<Map<String, String>> rows = read("A,B\n\"open,1\n3,4\n");
    assertThat(rows.get(0)).containsExactlyEntriesOf(Map.of("A", "open,1"));
    assertThat(rows.get(1)).containsEntry("A", "3").containsEntry("B", "4");
  }

  @Test
  void textAfterAClosingQuoteIsKept() throws IOException {
    assertThat(read("A,B\n\"a\" ,b\n").get(0)).containsEntry("A", "a ").containsEntry("B", "b");
  }

  @Test
  void emptyFieldsAndLeadingSpacesInTheHeader() throws IOException {
    assertThat(read("A,B\n,\n").get(0)).containsEntry("A", "").containsEntry("B", "");
    assertThat(read(" A,B\n1,2\n").get(0)).containsEntry(" A", "1");
  }

  @Test
  void lastLineWithoutNewlineIsRead() throws IOException {
    assertThat(read("A\n1\n2")).hasSize(2);
  }

  @Test
  void repeatedHeaderNameKeepsTheLastValue() throws IOException {
    assertThat(read("A,A\n1,2\n").get(0)).containsExactlyEntriesOf(Map.of("A", "2"));
  }

  @ParameterizedTest(name = "toInt(\"{0}\") = {1}")
  @CsvSource(delimiter = '|', value = {
      "2147483648|-2147483648", "-2147483649|2147483647", "99999999999|1215752191", "'\t+7'|7", "' -0'|0"})
  void toIntWrapsLikeAtoi(String in, int want) {
    assertThat(TelemetryCsv.toInt(in)).isEqualTo(want);
  }

  @Test
  void latLonAreStoredWithPrintfSixDecimals() {
    assertThat(MediationDrop.storedReal("43.1011014")).isEqualTo(43.101101);
    assertThat(MediationDrop.storedReal("-71.3731855")).isEqualTo(-71.373186);
    assertThat(MediationDrop.storedReal("")).isEqualTo(0.0);
    assertThat(MediationDrop.storedReal("1e-7")).isEqualTo(0.0);
  }

  @Test
  void malformedUtf8DoesNotAbortTheFile(@TempDir Path dir) throws Exception {
    Path csv = dir.resolve("usage.csv");
    Files.write(csv, new byte[] {'A', '\n', (byte) 0xC3, '(', '\n', 'o', 'k', '\n'});
    assertThat(TelemetryCsv.read(csv)).containsExactly(Map.of("A", "\uFFFD("), Map.of("A", "ok"));
  }

  /** Values printed by glibc atof for the same strings. */
  @Test
  void toDoubleReadsHexFloatsLikeAtof() {
    String[] in = {"0x1.8p5", "0x1.8", "0x.8", "0x", "0xg", "-0X1P-2", " +0x10p", "0x1p+3x", "0x.p1", "0xAbC.dEp-1"};
    double[] atof = {48, 1.5, 0.5, 0, 0, -0.25, 16, 8, 0, 1374.43359375};
    for (int i = 0; i < in.length; i++) assertThat(TelemetryCsv.toDouble(in[i])).as(in[i]).isEqualTo(atof[i]);
  }
}
