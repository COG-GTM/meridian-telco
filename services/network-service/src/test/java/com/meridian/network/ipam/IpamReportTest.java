package com.meridian.network.ipam;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

/**
 * Edge cases beyond the checked-in device file. edge-devices.expected.txt is the stdout of
 * {@code g++ network/ipam.cpp && ipam --devices edge-devices.csv} (duplicates, quoted fields, blank
 * lines, short rows, truncated and out-of-range addresses).
 */
class IpamReportTest {

  static Path resource(String name) throws Exception {
    return Path.of(IpamReportTest.class.getResource("/ipam/" + name).toURI());
  }

  @Test
  void edgeCaseDeviceFileMatchesLegacyBinaryOutput() throws Exception {
    String want = Files.readString(resource("edge-devices.expected.txt"));
    assertThat(IpamReport.legacyText(resource("edge-devices.csv"))).isEqualTo(want);
  }

  @Test
  void summaryCarriesTheLegacyFigures() throws Exception {
    IpamReport.Summary s = IpamReport.summarize(NetworkFiles.readDevices(resource("edge-devices.csv")));
    assertThat(s.devices()).isEqualTo(9);
    assertThat(s.addressesInUse()).isEqualTo(7);
    assertThat(s.duplicates()).containsExactly(
        new IpamReport.Duplicate("10.20.4.17", "core-dup"),
        new IpamReport.Duplicate("10.20.251.1", "lab-dup"));
    assertThat(s.reservedRange()).isEqualTo("10.20.250.0/24");
    assertThat(s.addressesInsideReserved()).isEqualTo(1);
    assertThat(s.subnets()).extracting(IpamReport.Subnet::subnet)
        .containsExactly("10.20.0.0/24", "10.20.250.0/24", "10.20.251.0/24", "10.20.4.0/24", "10.20.44.0/24");
  }

  @Test
  void legacyAddressParsingFollowsSscanf() {
    assertThat(IpamReport.legacyIpToU32("10.20.4.17")).isEqualTo(0x0A140411);
    assertThat(IpamReport.legacyIpToU32("10.20")).isEqualTo(0x0A140000);
    assertThat(IpamReport.legacyIpToU32("")).isZero();
    assertThat((IpamReport.legacyIpToU32("10.20.300.1") >>> 8) & 0xff).isEqualTo(44);
  }

  @Test
  void missingOrEmptyDeviceFileFailsLikeLegacy(@TempDir Path tmp) throws IOException {
    assertThatThrownBy(() -> IpamReport.legacyText(tmp.resolve("absent.csv")))
        .isInstanceOf(IllegalStateException.class).hasMessageStartingWith("no device file at ");
    Path headerOnly = Files.writeString(tmp.resolve("devices.csv"), "device_name,mgmt_ip\n");
    assertThatThrownBy(() -> IpamReport.legacyText(headerOnly)).isInstanceOf(IllegalStateException.class);
  }
}
