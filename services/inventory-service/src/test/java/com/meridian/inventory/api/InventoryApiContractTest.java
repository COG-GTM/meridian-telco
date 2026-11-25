package com.meridian.inventory.api;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.ContractJson;
import com.meridian.contracts.testkit.HttpContract;
import java.util.List;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.ResponseEntity;

/**
 * Frozen /sites /circuits /capacity contract (legacy bin/inventory-api :8081), contracts/http/.
 * Same status, and a body with the same keys, key order and number literals.
 */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class InventoryApiContractTest {

  @Autowired TestRestTemplate http;

  static List<HttpContract> contracts() {
    return HttpContract.withPrefix("sites", "circuits", "capacity", "inventory-");
  }

  @ParameterizedTest(name = "{0}")
  @MethodSource("contracts")
  void matchesFrozenContract(HttpContract c) {
    ResponseEntity<String> res = http.getForEntity(c.requestPath(), String.class);
    assertThat(res.getStatusCode().value()).as("status").isEqualTo(c.status());
    assertThat(res.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    ContractJson.assertMatches(c.body(), res.getBody());
  }
}
