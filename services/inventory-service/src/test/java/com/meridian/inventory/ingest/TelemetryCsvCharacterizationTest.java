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
 * Characterization of mediation/csv.h as used by mediation/ingest.cpp. Expected values were
 * observed by running csv.h read_csv/to_int on the same inputs (g++ probe, 2026-11-25).
 */
class TelemetryCsvCharacterizationTest {

  @TempDir Path dir;

  Path write(String content) throws IOException {
    Path f = dir.resolve("drop.csv");
    Files.writeString(f, content);
    return f;
  }

  @Test
  void headerNamesTheFieldsAndQuotedCommasStayInTheField() throws IOException {
    List<Map<String, String>> rows = TelemetryCsv.read(write("A,B,C\n1,\"x, y\",3\n"));
    assertThat(rows).containsExactly(Map.of("A", "1", "B", "x, y", "C", "3"));
  }

  @Test
  void blankLinesAreSkipped() throws IOException {
    assertThat(TelemetryCsv.read(write("A,B\n1,2\n\n3,4\n"))).hasSize(2);
  }

  @Test
  void shortRowsOmitTheMissingFieldsAndExtraColumnsAreDropped() throws IOException {
    List<Map<String, String>> rows = TelemetryCsv.read(write("A,B,C\n4,5\n6,7,8,9\n"));
    assertThat(rows.get(0)).containsExactlyInAnyOrderEntriesOf(Map.of("A", "4", "B", "5"));
    assertThat(rows.get(1)).containsExactlyInAnyOrderEntriesOf(Map.of("A", "6", "B", "7", "C", "8"));
  }

  @Test
  void emptyTrailingFieldIsPresentAndEmpty() throws IOException {
    assertThat(TelemetryCsv.read(write("A,B,C\n1,-7,\n")).get(0)).containsEntry("C", "");
  }

  @Test
  void leadingWhitespaceIsKept() throws IOException {
    assertThat(TelemetryCsv.read(write("A,B\n\" 12abc\",2\n")).get(0)).containsEntry("A", " 12abc");
  }

  @Test
  void missingFileYieldsNoRows() {
    assertThat(TelemetryCsv.read(dir.resolve("absent.csv"))).isEmpty();
  }

  @Test
  void headerOnlyFileYieldsNoRows() throws IOException {
    assertThat(TelemetryCsv.read(write("A,B\n"))).isEmpty();
  }

  @ParameterizedTest(name = "toInt(\"{0}\") = {1}")
  @CsvSource(delimiter = '|', emptyValue = "", value = {
      "1|1", "-7|-7", "' 12abc'|12", "1.9|1", "0x10|0", "''|0", "abc|0", "+5|5"})
  void toIntFollowsAtoi(String in, int want) {
    assertThat(TelemetryCsv.toInt(in)).isEqualTo(want);
  }

  @Test
  void realDropsParseLikeTheLegacyIngest() {
    Path data = com.meridian.contracts.testkit.MeridianRoot.resolve("data");
    assertThat(TelemetryCsv.read(data.resolve("sites.csv"))).hasSize(412);
    assertThat(TelemetryCsv.read(data.resolve("circuits.csv"))).hasSize(240);
    assertThat(TelemetryCsv.read(data.resolve("locations.csv"))).hasSize(22);
    assertThat(TelemetryCsv.read(data.resolve("usage.csv"))).hasSize(400);
    Map<String, String> first = TelemetryCsv.read(data.resolve("sites.csv")).get(0);
    assertThat(first).containsEntry("ASSET_ID", "100001").containsEntry("LAT", "43.101101")
        .containsEntry("TOTAL_CAP_MBPS", "10000").containsEntry("ALLOC_CAP_MBPS", "4300");
  }
}
