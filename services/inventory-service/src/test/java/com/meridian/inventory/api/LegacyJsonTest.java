package com.meridian.inventory.api;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

class LegacyJsonTest {

  /** Expected text is what sqlite3 3.37 prints for the same REAL values. */
  @ParameterizedTest(name = "{0} -> {1}")
  @CsvSource({"43.101101,43.101101", "-71.373185,-71.373185", "0.0,0.0", "-0.0,0.0", "0.00001,1.0e-05",
      "1e15,1.0e+15", "123456789012345678.0,1.23456789012346e+17", "0.0001,0.0001", "100.5,100.5",
      "12.0,12.0", "0.1234567,0.1234567"})
  void realsRenderLikeSqlite(double d, String want) {
    assertThat(LegacyJson.sqliteReal(d)).isEqualTo(want);
  }

  @Test
  void onlyQuoteAndBackslashAreEscaped() {
    assertThat(LegacyJson.escape("a\"b\\c\td")).isEqualTo("a\\\"b\\\\c\td");
  }

  @Test
  void queryIsRawLastValueWinsAndBarePairsAreIgnored() {
    assertThat(LegacyQuery.parse("REGION_CD=NE%2D04&x&REGION_CD=NE-04&a=b=c"))
        .containsEntry("REGION_CD", "NE-04").containsEntry("a", "b=c").doesNotContainKey("x");
  }
}
