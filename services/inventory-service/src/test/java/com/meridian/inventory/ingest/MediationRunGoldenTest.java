package com.meridian.inventory.ingest;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import java.io.IOException;
import java.nio.file.Files;
import org.junit.jupiter.api.Test;

/** bin/mediation --data data prints contracts/mediation/mediation-run.txt; the port must agree. */
class MediationRunGoldenTest {

  @Test
  void summaryOfTheCheckedInDropMatchesLegacy() throws IOException {
    String want = Files.readString(MeridianRoot.resolve("contracts/mediation/mediation-run.txt"));
    assertThat(MediationRun.summarize(MeridianRoot.resolve("data")).legacyReport()).isEqualTo(want);
  }
}
