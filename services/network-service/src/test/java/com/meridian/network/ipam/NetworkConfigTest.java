package com.meridian.network.ipam;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import java.nio.file.Path;
import java.util.List;
import org.junit.jupiter.api.Test;

/** The conf files bin/ipam ignores, parsed and cross-checked against the checked-in devices.csv. */
class NetworkConfigTest {

  static final Path NETWORK = MeridianRoot.resolve("network");

  final List<Device> devices = NetworkFiles.readDevices(NETWORK.resolve(NetworkFiles.DEVICES));
  final List<ReservedRange> ranges = NetworkFiles.readReservedRanges(NETWORK.resolve(NetworkFiles.RESERVED_RANGES));
  final List<SnmpTarget> targets = NetworkFiles.readSnmpTargets(NETWORK.resolve(NetworkFiles.SNMP_POLLING));

  @Test
  void parsesDevicesInFileOrder() {
    assertThat(devices).hasSize(147);
    assertThat(devices.getFirst()).isEqualTo(new Device(1, "Core-Router-NE04", "NE-04", "core-router",
        "10.20.4.17", "MSITE-104432", false));
    assertThat(devices).extracting(Device::seq).doesNotHaveDuplicates().startsWith(1, 2, 3);
  }

  @Test
  void parsesReservedRangesWithPurpose() {
    assertThat(ranges).containsExactly(
        new ReservedRange("10.20.250.0/24", "planned-agg-buildout"),
        new ReservedRange("10.20.251.0/24", "lab"));
  }

  @Test
  void parsesSnmpTargetsPositionallyWithoutTheCommunity() {
    assertThat(targets).hasSize(138);
    assertThat(targets.getFirst()).isEqualTo(new SnmpTarget(0, "10.20.4.17", 60, "Core-Router-NE04"));
    assertThat(targets.getLast()).isEqualTo(new SnmpTarget(137, "10.20.250.35", 60, "planned-agg-06"));
    assertThat(targets).extracting(SnmpTarget::host).doesNotContain("meridian-ro");
  }

  @Test
  void crossChecksConfAgainstDevices() {
    ConfigAnalysis a = ConfigAnalysis.of(devices, ranges, targets);
    assertThat(a.reservedRanges()).containsExactly(
        new ConfigAnalysis.RangeUsage("10.20.250.0/24", "planned-agg-buildout", 6),
        new ConfigAnalysis.RangeUsage("10.20.251.0/24", "lab", 0));
    assertThat(a.devicesInReservedRanges()).extracting(ConfigAnalysis.DeviceInRange::deviceName)
        .containsExactly("planned-agg-01", "planned-agg-02", "planned-agg-03", "planned-agg-04",
            "planned-agg-05", "planned-agg-06");
    assertThat(a.snmpTargets()).isEqualTo(138);
    // 147 devices, 138 targets: the nine unpolled devices are all OLTs
    assertThat(a.devicesNotPolled()).hasSize(9).allMatch(d -> d.role().equals("olt"));
    assertThat(a.devicesNotPolled()).extracting(ConfigAnalysis.DeviceRef::mgmtIp).containsExactly(
        "10.20.4.29", "10.20.4.47", "10.20.4.53", "10.20.5.29", "10.20.5.35", "10.20.5.41",
        "10.20.12.20", "10.20.12.32", "10.20.13.38");
    assertThat(a.targetsWithoutDevice()).isEmpty();
    assertThat(a.targetHostMismatches()).isEmpty();
  }

  @Test
  void flagsOrphanTargetsAndHostMismatches() {
    List<Device> two = List.of(
        new Device(1, "a", "NE-04", "olt", "10.20.1.1", "S", false),
        new Device(2, "b", "NE-04", "olt", "10.20.251.7", "S", false));
    List<SnmpTarget> t = List.of(
        new SnmpTarget(0, "10.20.1.1", 60, "renamed-a"),
        new SnmpTarget(1, "10.20.9.9", 60, "gone"));
    ConfigAnalysis a = ConfigAnalysis.of(two, ranges, t);
    assertThat(a.targetsWithoutDevice()).containsExactly(t.get(1));
    assertThat(a.targetHostMismatches()).containsExactly(
        new ConfigAnalysis.HostMismatch(0, "10.20.1.1", "renamed-a", "a"));
    assertThat(a.devicesNotPolled()).extracting(ConfigAnalysis.DeviceRef::deviceName).containsExactly("b");
    assertThat(a.devicesInReservedRanges()).containsExactly(
        new ConfigAnalysis.DeviceInRange("b", "10.20.251.7", "10.20.251.0/24", "lab"));
  }
}
