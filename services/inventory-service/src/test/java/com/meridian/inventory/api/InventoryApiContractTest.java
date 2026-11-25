package com.meridian.inventory.api;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.ContractJson;
import com.meridian.contracts.testkit.HttpContract;
import java.net.URI;
import java.util.List;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.MethodSource;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;

/**
 * Frozen /sites /circuits /capacity contract (legacy bin/inventory-api :8081), contracts/http/.
 * Requests are sent byte-for-byte (no client re-encoding of %-escapes). Same status, Content-Type and CORS header, and a body with the same keys, key order and number literals.
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
    ResponseEntity<String> res = http.getForEntity(URI.create(http.getRootUri() + c.requestPath()), String.class);
    assertThat(res.getStatusCode().value()).as("status").isEqualTo(c.status());
    assertThat(res.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    assertThat(res.getHeaders().getContentType()).as("httpd.h always sends application/json")
        .isEqualTo(MediaType.APPLICATION_JSON);
    ContractJson.assertMatches(c.body(), res.getBody());
  }
}
