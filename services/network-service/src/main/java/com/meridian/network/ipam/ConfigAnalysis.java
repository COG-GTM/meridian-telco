package com.meridian.network.ipam;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * What the legacy report does not cover: reserved-ranges.conf and snmp-polling.conf cross-checked
 * against devices.csv (the conf files are hand-edited copies of the device file and drift).
 */
public record ConfigAnalysis(
    List<RangeUsage> reservedRanges,
    int snmpTargets,
    List<DeviceInRange> devicesInReservedRanges,
    List<DeviceRef> devicesNotPolled,
    List<SnmpTarget> targetsWithoutDevice,
    List<HostMismatch> targetHostMismatches) {

  public record RangeUsage(String cidr, String purpose, int devicesInside) {}

  public record DeviceInRange(String deviceName, String mgmtIp, String cidr, String purpose) {}

  public record DeviceRef(String deviceName, String mgmtIp, String region, String role) {}

  /** The target's ip belongs to a device but the poller host label names something else. */
  public record HostMismatch(int position, String ip, String targetHost, String deviceName) {}

  public static ConfigAnalysis of(List<Device> devices, List<ReservedRange> ranges, List<SnmpTarget> targets) {
    List<RangeUsage> usage = new ArrayList<>();
    List<DeviceInRange> inRanges = new ArrayList<>();
    for (ReservedRange r : ranges) {
      Ipv4.Cidr cidr = Ipv4.Cidr.parse(r.cidr());
      int inside = 0;
      if (cidr != null) {
        for (Device d : devices) {
          if (cidr.contains(d.mgmtIp())) {
            inside++;
            inRanges.add(new DeviceInRange(d.deviceName(), d.mgmtIp(), r.cidr(), r.purpose()));
          }
        }
      }
      usage.add(new RangeUsage(r.cidr(), r.purpose(), inside));
    }

    Set<String> polled = new HashSet<>();
    targets.forEach(t -> polled.add(t.ip()));
    List<DeviceRef> notPolled = devices.stream()
        .filter(d -> !polled.contains(d.mgmtIp()))
        .map(d -> new DeviceRef(d.deviceName(), d.mgmtIp(), d.region(), d.role()))
        .toList();

    Map<String, List<Device>> byIp = new HashMap<>();
    devices.forEach(d -> byIp.computeIfAbsent(d.mgmtIp(), k -> new ArrayList<>()).add(d));
    List<SnmpTarget> orphans = new ArrayList<>();
    List<HostMismatch> mismatches = new ArrayList<>();
    for (SnmpTarget t : targets) {
      switch (byIp.get(t.ip())) {
        case null -> orphans.add(t);
        case List<Device> ds when t.host() != null
            && ds.stream().noneMatch(d -> t.host().equals(d.deviceName())) ->
            mismatches.add(new HostMismatch(t.position(), t.ip(), t.host(), ds.getFirst().deviceName()));
        case List<Device> ds -> { }
      }
    }
    return new ConfigAnalysis(List.copyOf(usage), targets.size(), List.copyOf(inRanges), notPolled,
        List.copyOf(orphans), List.copyOf(mismatches));
  }
}
