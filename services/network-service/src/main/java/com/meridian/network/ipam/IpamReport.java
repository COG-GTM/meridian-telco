package com.meridian.network.ipam;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeMap;

/** network/ipam.cpp: management address report over network/devices.csv (10.20.0.0/16). */
public final class IpamReport {

  public static final String SUPERNET = "10.20.0.0/16";
  /** ipam.cpp hardcodes this range (third octet 250); it does not read reserved-ranges.conf. */
  public static final String LEGACY_RESERVED = "10.20.250.0/24";
  private static final int LEGACY_RESERVED_OCTET = 250;

  public record Duplicate(String mgmtIp, String deviceName) {}

  public record Subnet(String subnet, int devices) {}

  /** Exactly the figures bin/ipam prints, in the order it prints them. */
  public record Summary(String supernet, int devices, int addressesInUse, int internalDuplicates,
      List<Duplicate> duplicates, String reservedRange, int addressesInsideReserved,
      List<Subnet> subnets) {}

  private IpamReport() {}

  /** The exact stdout of bin/ipam --devices devicesCsv (contracts/ipam/ipam-report.txt). */
  public static String legacyText(Path devicesCsv) {
    List<Device> devices = NetworkFiles.readDevices(devicesCsv);
    if (devices.isEmpty()) {
      throw new IllegalStateException("no device file at " + devicesCsv);
    }
    return legacyText(devices);
  }

  public static String legacyText(List<Device> devices) {
    return text(summarize(devices));
  }

  public static Summary summarize(List<Device> devices) {
    // std::map<std::string,int>: byte-wise key order, so 10.20.250.0/24 sorts before 10.20.28.0/24
    TreeMap<String, Integer> perThirdOctet = new TreeMap<>();
    Set<String> seen = new HashSet<>();
    List<Duplicate> dupes = new ArrayList<>();
    int reserved = 0;
    for (Device d : devices) {
      String ip = d.mgmtIp();
      if (!seen.add(ip)) dupes.add(new Duplicate(ip, d.deviceName()));
      int octet = (legacyIpToU32(ip) >>> 8) & 0xff;
      perThirdOctet.merge("10.20." + octet + ".0/24", 1, Integer::sum);
      if (octet == LEGACY_RESERVED_OCTET) reserved++;
    }
    List<Subnet> subnets = perThirdOctet.entrySet().stream()
        .map(e -> new Subnet(e.getKey(), e.getValue())).toList();
    return new Summary(SUPERNET, devices.size(), seen.size(), dupes.size(), List.copyOf(dupes),
        LEGACY_RESERVED, reserved, subnets);
  }

  public static String text(Summary s) {
    StringBuilder out = new StringBuilder();
    for (Duplicate d : s.duplicates()) {
      out.append("DUPLICATE ").append(d.mgmtIp()).append(' ').append(d.deviceName()).append('\n');
    }
    out.append("supernet ").append(s.supernet()).append('\n');
    out.append("devices ").append(s.devices()).append(", addresses in use ").append(s.addressesInUse())
        .append(", internal duplicates ").append(s.internalDuplicates()).append('\n');
    out.append("addresses inside reserved ").append(s.reservedRange()).append(": ")
        .append(s.addressesInsideReserved()).append('\n');
    for (Subnet n : s.subnets()) {
      out.append(String.format(Locale.ROOT, "  %-18s %d", n.subnet(), n.devices())).append('\n');
    }
    return out.toString();
  }

  /**
   * ipam.cpp's {@code sscanf(ip, "%u.%u.%u.%u")} packed into 32 bits: missing or unparsable octets
   * stay 0 and out-of-range octets bleed into their neighbours, exactly as the C code does.
   */
  static int legacyIpToU32(String ip) {
    int[] v = new int[4];
    int pos = 0;
    for (int i = 0; i < 4; i++) {
      if (i > 0) {
        if (pos >= ip.length() || ip.charAt(pos) != '.') break;
        pos++;
      }
      while (pos < ip.length() && Character.isWhitespace(ip.charAt(pos))) pos++;
      boolean negative = false;
      if (pos < ip.length() && (ip.charAt(pos) == '+' || ip.charAt(pos) == '-')) {
        negative = ip.charAt(pos) == '-';
        pos++;
      }
      int start = pos;
      long n = 0;
      boolean overflow = false;
      while (pos < ip.length() && ip.charAt(pos) >= '0' && ip.charAt(pos) <= '9') {
        int digit = ip.charAt(pos) - '0';
        if (!overflow) {
          long hi = Math.unsignedMultiplyHigh(n, 10L);
          long next = n * 10 + digit;
          if (hi != 0 || Long.compareUnsigned(next, n * 10) < 0) overflow = true;
          else n = next;
        }
        pos++;
      }
      if (pos == start) break;
      // glibc converts %u with strtoul (64-bit unsigned long, saturating) and narrows to unsigned int
      long u = overflow ? -1L : (negative ? -n : n);
      v[i] = (int) u;
    }
    return (v[0] << 24) | (v[1] << 16) | (v[2] << 8) | v[3];
  }
}
