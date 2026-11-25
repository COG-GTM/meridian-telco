package com.meridian.inventory.ingest;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.Mockito.when;

import com.meridian.contracts.events.UsageRecorded;
import com.meridian.inventory.events.UsagePublisher;
import com.meridian.inventory.store.InventoryStore;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.test.context.bean.override.mockito.MockitoBean;

/** What reaches billing when acknowledgements go missing or a drop repeats an (acct, period). */
@SpringBootTest(properties = "meridian.ingest.on-startup=false")
class IngestPublishTest {

  @MockitoBean UsagePublisher publisher;
  @Autowired IngestService ingest;
  @Autowired InventoryStore store;
  @Autowired JdbcTemplate jdbc;
  @TempDir Path dir;

  final List<List<UsageRecorded>> calls = new ArrayList<>();
  boolean brokerUp;

  @BeforeEach
  void publisherRecordsAndAcksOnlyWhenTheBrokerIsUp() {
    jdbc.update("DELETE FROM usage_record");
    when(publisher.enabled()).thenReturn(true);
    when(publisher.publish(anyList())).thenAnswer(inv -> {
      List<UsageRecorded> sent = inv.getArgument(0);
      calls.add(sent);
      return brokerUp ? sent : List.of();
    });
  }

  Path drop(String name, String usageCsv) throws Exception {
    Path d = Files.createDirectories(dir.resolve(name));
    Files.writeString(d.resolve("sites.csv"), "ASSET_ID,STATUS_CD\n1,1\n");
    Files.writeString(d.resolve("usage.csv"), usageCsv);
    return d;
  }

  @Test
  void unacknowledgedUsageIsResentByTheNextRunEvenIfTheDropRotated() throws Exception {
    brokerUp = false;
    ingest.run(drop("june", "ACCT_ID,PERIOD,USAGE_MB\nA-1,2026-06,10\n"));
    assertThat(store.usage()).singleElement().satisfies(u -> assertThat(u.publishedAt()).isNull());

    brokerUp = true;
    ingest.run(drop("july", "ACCT_ID,PERIOD,USAGE_MB\nA-1,2026-07,20\n"));
    assertThat(calls.get(1)).extracting(UsageRecorded::key).containsExactly("A-1|2026-06", "A-1|2026-07");
    assertThat(store.usage()).allSatisfy(u -> assertThat(u.publishedAt()).isNotNull());
  }

  @Test
  void aRepeatedAccountPeriodIsPublishedOnceWithTheStoredCorrection() throws Exception {
    brokerUp = true;
    ingest.run(drop("dup", "ACCT_ID,PERIOD,USAGE_MB\nB-2,2026-06,10\nB-2,2026-06,15\n"));
    assertThat(calls.get(calls.size() - 1)).singleElement()
        .satisfies(e -> assertThat(e.usageMb()).isEqualTo(15L));
    assertThat(store.usage()).filteredOn(u -> u.acctId().equals("B-2"))
        .singleElement().satisfies(u -> assertThat(u.publishedAt()).isNotNull());
  }

  @Test
  void anAcknowledgementForAnOlderValueDoesNotMarkTheCorrection() throws Exception {
    brokerUp = false;
    ingest.run(drop("v1", "ACCT_ID,PERIOD,USAGE_MB\nC-3,2026-06,10\n"));
    UsageRecorded stale = calls.get(calls.size() - 1).get(0);
    ingest.run(drop("v2", "ACCT_ID,PERIOD,USAGE_MB\nC-3,2026-06,99\n"));
    store.markPublished(List.of(stale), java.time.Instant.now());
    assertThat(store.usage()).filteredOn(u -> u.acctId().equals("C-3"))
        .singleElement().satisfies(u -> assertThat(u.publishedAt()).isNull());
  }
}
