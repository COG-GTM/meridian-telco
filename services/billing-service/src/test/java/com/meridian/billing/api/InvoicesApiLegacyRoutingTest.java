package com.meridian.billing.api;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.ContractJson;
import com.meridian.contracts.testkit.HttpContract;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpMethod;
import org.springframework.http.ResponseEntity;

/** httpd.h routing quirks the dashboard may rely on: prefix match, last query value, any Accept. */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class InvoicesApiLegacyRoutingTest {

  @Autowired TestRestTemplate http;

  @Test
  void repeatedQueryKeyKeepsTheLastValue() {
    ResponseEntity<String> res = http.getForEntity("/invoices?acct=NOPE-000000&acct=BEACON-004417&period=2026-07", String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(200);
    ContractJson.assertMatches(HttpContract.load("invoices-acct-period").body(), res.getBody());
  }

  @Test
  void commaInAFilterIsPartOfTheValue() {
    ResponseEntity<String> res = http.getForEntity("/invoices?acct=NOPE-000000,BEACON-004417", String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(404);
    assertThat(res.getBody()).isEqualTo("{\"invoices\":[]}");
  }

  @Test
  void filtersAreNotPercentDecoded() {
    ResponseEntity<String> res = http.getForEntity(
        java.net.URI.create(http.getRootUri() + "/invoices?acct=%42EACON-004417"), String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(404);
    assertThat(res.getBody()).isEqualTo("{\"invoices\":[]}");
  }

  @Test
  void formBodyIsNotAFilter() {
    HttpHeaders h = new HttpHeaders();
    h.setContentType(org.springframework.http.MediaType.APPLICATION_FORM_URLENCODED);
    ResponseEntity<String> res = http.exchange("/invoices?period=2026-07", HttpMethod.POST,
        new HttpEntity<>("acct=NOPE-000000", h), String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(200);
    assertThat(res.getBody()).contains("BEACON-004417");
  }

  @Test
  void anyAcceptHeaderGetsJson() {
    HttpHeaders h = new HttpHeaders();
    h.set(HttpHeaders.ACCEPT, "text/html");
    ResponseEntity<String> res = http.exchange("/invoices?acct=NOPE-000000", HttpMethod.GET, new HttpEntity<>(h), String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(404);
    assertThat(res.getHeaders().getContentType()).hasToString("application/json");
    assertThat(res.getBody()).isEqualTo("{\"invoices\":[]}");
  }

  @Test
  void pathPrefixRoutesToInvoices() {
    ResponseEntity<String> res = http.getForEntity("/invoices-v1/current?acct=NOPE-000000", String.class);
    assertThat(res.getStatusCode().value()).isEqualTo(404);
    assertThat(res.getBody()).isEqualTo("{\"invoices\":[]}");
    assertThat(res.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
  }
}
