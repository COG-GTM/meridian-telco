package com.meridian.network.ipam;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import java.io.IOException;
import java.nio.file.Files;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;

/**
 * bin/ipam prints contracts/ipam/ipam-report.txt. Note the legacy report hardcodes the reserved
 * 10.20.250.0/24 range and currently counts 6 devices inside it.
 */
@Tag("pending")
class IpamReportGoldenTest {

  @Test
  void reportOfTheCheckedInDeviceFileMatchesLegacy() throws IOException {
    String want = Files.readString(MeridianRoot.resolve("contracts/ipam/ipam-report.txt"));
    assertThat(IpamReport.legacyText(MeridianRoot.resolve("network/devices.csv"))).isEqualTo(want);
  }
}
