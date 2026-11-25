package com.meridian.billing.store;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.billing.batch.RegisterJob;
import com.meridian.billing.model.Invoice;
import com.meridian.billing.model.UsageRecord;
import com.meridian.contracts.events.UsageRecorded;
import com.meridian.contracts.testkit.MeridianRoot;
import java.math.BigDecimal;
import java.nio.file.Files;
import java.time.Instant;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.jdbc.core.simple.JdbcClient;

/** Repositories and the DB-backed register job, against Testcontainers Postgres seeded from legacy files. */
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.NONE)
class BillingStoreTest {

  @Autowired AccountRepository accounts;
  @Autowired UsageRepository usage;
  @Autowired RegisterRepository registers;
  @Autowired RegisterJob job;
  @Autowired JdbcClient jdbc;

  @AfterEach
  void dropTestRows() {
    jdbc.sql("DELETE FROM usage_record WHERE acct_id LIKE 'TEST-%'").update();
  }

  @Test
  void seededFromLegacyFiles() {
    assertThat(accounts.count()).isEqualTo(200);
    assertThat(usage.findAll()).hasSize(400).first()
        .isEqualTo(new UsageRecord("BEACON-004417", "2026-06", 5813259));
    assertThat(accounts.find("MER-012267")).get()
        .satisfies(a -> assertThat(a.province()).isEqualTo("ON"));
  }

  @Test
  void registerFromTheStoreMatchesTheFrozenFeedAndIsRecorded() throws Exception {
    var all = job.run(null, false);
    assertThat(all.text()).isEqualTo(Files.readString(MeridianRoot.resolve("contracts/register/register-all.txt")));
    var june = job.run("2026-06", true);
    assertThat(june.text()).isEqualTo(Files.readString(MeridianRoot.resolve("contracts/register/register-2026-06.jsonl")));

    assertThat(registers.invoices(all.registerRunId())).hasSize(400)
        .containsExactlyElementsOf(all.register().invoices());
    assertThat(registers.invoices(june.registerRunId())).hasSize(200);
    assertThat(registers.grandTotal(all.registerRunId())).isEqualTo(all.register().grandTotal());
    Invoice mer = registers.invoices(june.registerRunId()).stream()
        .filter(i -> i.acctId().equals("MER-012267")).findFirst().orElseThrow();
    assertThat(mer.federalTax()).isEqualTo(new BigDecimal("143.32"));
    assertThat(mer.total()).isEqualTo(new BigDecimal("1201.72"));
  }

  @Test
  void usageUpsertIsIdempotentAndIgnoresOlderRecords() {
    Instant t0 = Instant.parse("2026-07-01T02:00:00Z");
    var first = new UsageRecorded("TEST-1", "2026-07", 100, "usage.csv", t0);
    assertThat(usage.upsert(first)).isEqualTo(1);
    long before = usage.count();
    usage.upsert(first);
    assertThat(usage.count()).isEqualTo(before);
    assertThat(usage.upsert(new UsageRecorded("TEST-1", "2026-07", 50, "usage.csv", t0.minusSeconds(60)))).isZero();
    usage.upsert(new UsageRecorded("TEST-1", "2026-07", 300, "usage.csv", t0.plusSeconds(60)));
    assertThat(usage.findAll()).filteredOn(u -> u.acctId().equals("TEST-1"))
        .containsExactly(new UsageRecord("TEST-1", "2026-07", 300));
    assertThat(usage.insertIfAbsent(new UsageRecord("TEST-1", "2026-07", 1), "file")).isZero();
  }
}
