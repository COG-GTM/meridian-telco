package com.meridian.inventory.capacity;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/**
 * mediation/capacity.cpp + locations.cpp. The sales desk rule is AVAIL = TOTAL - ALLOC with no
 * maintenance buffer (README, capacity.cpp RS 2009-11, locations.cpp "we do not hold anything back").
 */
@Tag("pending")
class CapacityRuleCharacterizationTest {

  final CapacityRule rule = new CapacityRule();

  @ParameterizedTest(name = "available({0}, {1}) = {2}")
  @CsvSource({"1000,600,400", "2000,1250,750", "4000,4000,0", "1000,1200,0", "0,0,0", "10000,4300,5700"})
  void availableIsTotalMinusAllocatedFlooredAtZero(int total, int alloc, int want) {
    assertThat(rule.available(total, alloc)).isEqualTo(want);
  }

  @Test
  void noMaintenanceBufferIsHeldBack() {
    assertThat(rule.available(1000, 0)).isEqualTo(1000);
    assertThat(rule.canSupport(1000, 600, 400)).isTrue();
    assertThat(rule.canSupport(1000, 600, 401)).isFalse();
  }

  @ParameterizedTest(name = "utilizationPct({0}, {1}) = {2}")
  @CsvSource({"1000,600,60", "3000,1000,33", "3000,2000,66", "0,500,0", "-5,1,0", "1000,1200,120"})
  void utilizationIsIntegerDivisionTruncated(int total, int alloc, int want) {
    assertThat(rule.utilizationPct(total, alloc)).isEqualTo(want);
  }

  @Test
  void zeroRequestedIsAlwaysSupported() {
    assertThat(rule.canSupport(1000, 1200, 0)).isTrue();
  }
}
