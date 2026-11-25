package com.meridian.inventory.api;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.HttpContract;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;

/** Legacy httpd.h behaviour beyond the frozen exchanges. */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class InventoryApiBehaviourTest {

  @Autowired TestRestTemplate http;

  @Test
  void responsesAreJsonWithOpenCors() {
    ResponseEntity<String> res = http.getForEntity("/sites", String.class);
    assertThat(res.getHeaders().getContentType()).isEqualTo(MediaType.APPLICATION_JSON);
    assertThat(res.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    assertThat(http.getForEntity("/", String.class).getStatusCode().value()).isEqualTo(404);
  }

  @Test
  void routesMatchOnPathPrefixLikeHttpd() {
    assertThat(http.getForEntity("/sites/extra?REGION_CD=NE-04", String.class).getBody())
        .isEqualTo(HttpContract.load("sites-region-ne04").body());
    assertThat(http.getForEntity("/capacityX", String.class).getBody())
        .isEqualTo(HttpContract.load("capacity").body());
  }

  @Test
  void sqlInjectionInFiltersMatchesNothing() {
    assertThat(http.getForEntity("/sites?REGION_CD=x'%20OR%20'1'='1", String.class).getBody())
        .isEqualTo("{\"count\":0,\"sites\":[]}");
    assertThat(http.getForEntity("/sites?STATUS_CD=3%20OR%201=1", String.class).getBody())
        .isEqualTo("{\"count\":0,\"sites\":[]}");
    assertThat(http.getForEntity("/circuits?CIRCUIT_ID=x'%20OR%20'1'='1", String.class).getBody())
        .isEqualTo("{\"count\":0,\"active_count\":0,\"circuits\":[]}");
  }

  @Test
  void statusFilterTakesAnySqliteNumericLiteral() {
    String active = http.getForEntity("/sites?STATUS_CD=1", String.class).getBody();
    for (String v : new String[] {"1e0", "+1", "1.0", "0x1", ".1e1", "0X001"}) {
      assertThat(http.getForEntity("/sites?STATUS_CD=" + v, String.class).getBody()).as(v).isEqualTo(active);
    }
    assertThat(http.getForEntity("/sites?STATUS_CD=1.5", String.class).getBody()).isEqualTo("{\"count\":0,\"sites\":[]}");
    assertThat(http.getForEntity("/sites?STATUS_CD=0x" + "f".repeat(17), String.class).getBody())
        .isEqualTo("{\"count\":0,\"sites\":[]}");
    assertThat(http.getForEntity("/sites?STATUS_CD=1e99999999999", String.class).getBody())
        .isEqualTo("{\"count\":0,\"sites\":[]}");
  }

  @Test
  void bodiesAreByteIdenticalToTheFrozenContracts() {
    for (HttpContract c : HttpContract.withPrefix("sites", "circuits", "capacity", "inventory-")) {
      assertThat(http.getForEntity(c.requestPath(), String.class).getBody()).as(c.name()).isEqualTo(c.body());
    }
  }
}
