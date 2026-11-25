package com.meridian.network.api;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.meridian.contracts.testkit.MeridianRoot;
import com.meridian.network.ipam.NetworkFiles;
import com.meridian.network.store.NetworkLoader;
import com.meridian.network.store.NetworkStore;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpMethod;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;
import org.testcontainers.utility.DockerImageName;

/** network-service on its own PostgreSQL, loaded from network/ at startup. */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers(disabledWithoutDocker = true)
class NetworkServiceApiTest {

  static final DockerImageName PG = DockerImageName.parse(
      System.getProperty("meridian.postgres.image", "postgres:16-alpine")).asCompatibleSubstituteFor("postgres");

  @Container @ServiceConnection
  static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>(PG).withDatabaseName("network");

  @DynamicPropertySource
  static void networkDir(DynamicPropertyRegistry r) {
    r.add("meridian.network-dir", () -> MeridianRoot.resolve("network").toString());
  }

  @Autowired TestRestTemplate http;
  @Autowired JdbcTemplate jdbc;
  @Autowired NetworkStore store;
  final ObjectMapper json = new ObjectMapper();

  JsonNode getJson(String path) throws Exception {
    ResponseEntity<String> r = http.getForEntity(path, String.class);
    assertThat(r.getStatusCode()).isEqualTo(HttpStatus.OK);
    assertThat(r.getHeaders().getContentType().toString()).startsWith("application/json");
    assertThat(r.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    return json.readTree(r.getBody());
  }

  @Test
  void startupLoadsTheConfigFilesIntoPostgres() {
    assertThat(jdbc.queryForObject("SELECT count(*) FROM device", Integer.class)).isEqualTo(147);
    assertThat(jdbc.queryForObject("SELECT count(*) FROM reserved_range", Integer.class)).isEqualTo(2);
    assertThat(jdbc.queryForObject("SELECT count(*) FROM snmp_target", Integer.class)).isEqualTo(138);
  }

  @Test
  void legacyTextReportIsByteIdenticalToBinIpam() throws Exception {
    ResponseEntity<String> r = http.getForEntity("/ipam/report.txt", String.class);
    assertThat(r.getStatusCode()).isEqualTo(HttpStatus.OK);
    assertThat(r.getHeaders().getContentType().toString()).startsWith("text/plain");
    assertThat(r.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    assertThat(r.getBody()).isEqualTo(Files.readString(MeridianRoot.resolve("contracts/ipam/ipam-report.txt")));
  }

  @Test
  void jsonReportSeparatesLegacyFiguresFromConfAnalysis() throws Exception {
    JsonNode d = getJson("/ipam/report");
    assertThat(d.fieldNames()).toIterable().containsExactly("legacy", "config");
    JsonNode legacy = d.get("legacy");
    assertThat(legacy.get("supernet").asText()).isEqualTo("10.20.0.0/16");
    assertThat(legacy.get("devices").asInt()).isEqualTo(147);
    assertThat(legacy.get("addressesInUse").asInt()).isEqualTo(147);
    assertThat(legacy.get("internalDuplicates").asInt()).isZero();
    assertThat(legacy.get("reservedRange").asText()).isEqualTo("10.20.250.0/24");
    assertThat(legacy.get("addressesInsideReserved").asInt()).isEqualTo(6);
    assertThat(legacy.get("subnets")).hasSize(16);
    assertThat(legacy.get("subnets").get(6).get("subnet").asText()).isEqualTo("10.20.250.0/24");

    JsonNode config = d.get("config");
    assertThat(config.get("reservedRanges")).hasSize(2);
    assertThat(config.get("reservedRanges").get(1).get("purpose").asText()).isEqualTo("lab");
    assertThat(config.get("snmpTargets").asInt()).isEqualTo(138);
    assertThat(config.get("devicesInReservedRanges")).hasSize(6);
    assertThat(config.get("devicesNotPolled")).hasSize(9);
    assertThat(config.get("targetsWithoutDevice")).isEmpty();
    assertThat(config.get("targetHostMismatches")).isEmpty();
  }

  @Test
  void listsDevicesRangesAndTargets() throws Exception {
    JsonNode devices = getJson("/devices");
    assertThat(devices.get("count").asInt()).isEqualTo(147);
    assertThat(devices.get("devices").get(0).get("deviceName").asText()).isEqualTo("Core-Router-NE04");

    JsonNode ranges = getJson("/reserved-ranges");
    assertThat(ranges.get("count").asInt()).isEqualTo(2);
    assertThat(ranges.get("reservedRanges").get(0).get("cidr").asText()).isEqualTo("10.20.250.0/24");

    JsonNode targets = getJson("/snmp-targets");
    assertThat(targets.get("count").asInt()).isEqualTo(138);
    assertThat(targets.get("snmpTargets").get(0).get("position").asInt()).isZero();
    assertThat(targets.toString()).doesNotContain("community").doesNotContain("meridian-ro");
  }

  @Test
  void refreshReloadsFromTheConfigFiles() throws Exception {
    jdbc.update("DELETE FROM device WHERE seq > 1");
    assertThat(getJson("/devices").get("count").asInt()).isEqualTo(1);

    ResponseEntity<String> r = http.postForEntity("/ipam/refresh", null, String.class);
    assertThat(r.getStatusCode()).isEqualTo(HttpStatus.OK);
    JsonNode loaded = json.readTree(r.getBody());
    assertThat(loaded.get("devices").asInt()).isEqualTo(147);
    assertThat(loaded.get("reservedRanges").asInt()).isEqualTo(2);
    assertThat(loaded.get("snmpTargets").asInt()).isEqualTo(138);
    assertThat(getJson("/devices").get("count").asInt()).isEqualTo(147);
  }

  @Test
  void answersCorsPreflightFromTheDashboard() {
    HttpHeaders h = new HttpHeaders();
    h.setOrigin("http://localhost:8000");
    h.setAccessControlRequestMethod(HttpMethod.GET);
    ResponseEntity<String> r = http.exchange("/ipam/report", HttpMethod.OPTIONS, new HttpEntity<>(h), String.class);
    assertThat(r.getStatusCode().is2xxSuccessful()).isTrue();
    assertThat(r.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
  }

  @Test
  void preflightAllowsRequestHeaders() {
    HttpHeaders h = new HttpHeaders();
    h.setOrigin("http://localhost:8000");
    h.setAccessControlRequestMethod(HttpMethod.POST);
    h.setAccessControlRequestHeaders(List.of("Content-Type", "X-Requested-With"));
    ResponseEntity<String> r = http.exchange("/ipam/refresh", HttpMethod.OPTIONS, new HttpEntity<>(h), String.class);
    assertThat(r.getStatusCode().is2xxSuccessful()).isTrue();
    assertThat(r.getHeaders().getAccessControlAllowOrigin()).isEqualTo("*");
    assertThat(r.getHeaders().getAccessControlAllowHeaders())
        .anySatisfy(v -> assertThat(v).isIn("*", "Content-Type", "content-type"));
  }

  @Test
  void refreshWithAMissingConfFileLeavesTheStoreUntouched(@TempDir Path tmp) throws Exception {
    Path network = MeridianRoot.resolve("network");
    Files.copy(network.resolve(NetworkFiles.DEVICES), tmp.resolve(NetworkFiles.DEVICES));
    Files.copy(network.resolve(NetworkFiles.RESERVED_RANGES), tmp.resolve(NetworkFiles.RESERVED_RANGES));
    NetworkLoader loader = new NetworkLoader(store, tmp, false);
    assertThatThrownBy(loader::refresh).isInstanceOf(IllegalStateException.class)
        .hasMessageContaining(NetworkFiles.SNMP_POLLING);
    assertThat(jdbc.queryForObject("SELECT count(*) FROM snmp_target", Integer.class)).isEqualTo(138);
  }
}
