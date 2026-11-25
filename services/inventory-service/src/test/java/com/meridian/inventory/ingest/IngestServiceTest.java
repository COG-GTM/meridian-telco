package com.meridian.inventory.ingest;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import com.meridian.inventory.store.InventoryStore;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.jdbc.core.JdbcTemplate;

/** The startup ingest of data/ (test config) against Postgres, Kafka disabled. */
@SpringBootTest
class IngestServiceTest {

  @Autowired IngestService ingest;
  @Autowired InventoryStore store;
  @Autowired JdbcTemplate jdbc;

  @Test
  void dropIsStoredAndTheRunRecordsTheLegacyReport() throws Exception {
    assertThat(store.sites()).hasSize(412);
    assertThat(store.circuits()).hasSize(240);
    assertThat(store.locations()).hasSize(22);
    assertThat(store.usage()).hasSize(400).allSatisfy(u -> {
      assertThat(u.source()).isEqualTo(IngestService.USAGE_SOURCE);
      assertThat(u.publishedAt()).as("kafka disabled").isNull();
    });
    assertThat(store.usage().get(0).acctId()).isEqualTo("BEACON-004417");
    assertThat(store.usage().get(0).usageMb()).isEqualTo(5813259L);
    String report = jdbc.queryForObject("SELECT report FROM ingest_run ORDER BY id DESC LIMIT 1", String.class);
    assertThat(report).isEqualTo(Files.readString(MeridianRoot.resolve("contracts/mediation/mediation-run.txt")));
  }

  @Test
  void aDropWithoutSiteTelemetryOnlyRefreshesLocations(@TempDir Path drop) throws Exception {
    Files.writeString(drop.resolve("circuits.csv"), "CIRCUIT_ID\nX\n");
    Files.copy(MeridianRoot.resolve("data/locations.csv"), drop.resolve("locations.csv"));
    assertThat(ingest.run(drop)).isEmpty();
    assertThat(store.sites()).hasSize(412);
    assertThat(store.circuits()).hasSize(240);
    assertThat(store.locations()).hasSize(22);
  }

  @Test
  void aDropWithoutLocationsKeepsTheStoredOnes(@TempDir Path drop) {
    assertThat(ingest.run(drop)).isEmpty();
    assertThat(store.locations()).hasSize(22);
  }

  @Test
  void rerunningTheSameDropIsIdempotent() {
    assertThat(ingest.run()).hasValueSatisfying(r -> {
      assertThat(r.summary().sitesLoaded()).isEqualTo(412);
      assertThat(r.usagePublished()).isZero();
    });
    assertThat(store.sites()).hasSize(412);
    assertThat(store.usage()).hasSize(400);
  }
}
