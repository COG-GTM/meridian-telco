package com.meridian.contracts.testkit;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import org.junit.jupiter.api.Test;

class ContractJsonTest {

  @Test
  void whitespaceIsIgnored() {
    assertThat(ContractJson.firstDifference("{\"a\":1,\"b\":[true]}", "{ \"a\": 1, \"b\": [ true ] }"))
        .isNull();
  }

  @Test
  void numberLiteralsMustMatchAsWritten() {
    assertThat(ContractJson.firstDifference("{\"x\":640.00}", "{\"x\":640.0}")).contains("num:640.00");
  }

  @Test
  void keyOrderMatters() {
    assertThat(ContractJson.firstDifference("{\"a\":1,\"b\":2}", "{\"b\":2,\"a\":1}")).isNotNull();
  }

  @Test
  void missingTrailingFieldIsReported() {
    assertThatThrownBy(() -> ContractJson.assertMatches("{\"a\":1,\"b\":2}", "{\"a\":1}"))
        .isInstanceOf(AssertionError.class);
  }

  @Test
  void everyFrozenHttpContractIsLoadableJson() {
    var all = HttpContract.withPrefix("");
    assertThat(all).hasSizeGreaterThanOrEqualTo(15);
    for (HttpContract c : all) {
      assertThat(ContractJson.firstDifference(c.body(), c.body())).as(c.name()).isNull();
      assertThat(c.requestPath()).startsWith("/");
    }
  }
}
