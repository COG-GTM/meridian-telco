package com.meridian.network.ipam;

import java.util.OptionalLong;

/** Strict dotted-quad IPv4 helpers for the config analysis (the legacy report has its own parser). */
public final class Ipv4 {

  private Ipv4() {}

  public static OptionalLong parse(String ip) {
    if (ip == null) return OptionalLong.empty();
    String[] p = ip.strip().split("\\.", -1);
    if (p.length != 4) return OptionalLong.empty();
    long v = 0;
    for (String octet : p) {
      if (octet.isEmpty() || octet.length() > 3 || !octet.chars().allMatch(Character::isDigit)) {
        return OptionalLong.empty();
      }
      int o = Integer.parseInt(octet);
      if (o > 255) return OptionalLong.empty();
      v = (v << 8) | o;
    }
    return OptionalLong.of(v);
  }

  /** A parsed {@code a.b.c.d/len} block. */
  public record Cidr(long network, int prefix) {
    public static Cidr parse(String cidr) {
      if (cidr == null) return null;
      int slash = cidr.indexOf('/');
      String addr = slash < 0 ? cidr : cidr.substring(0, slash);
      int prefix;
      try {
        prefix = slash < 0 ? 32 : Integer.parseInt(cidr.substring(slash + 1).strip());
      } catch (NumberFormatException e) {
        return null;
      }
      OptionalLong ip = Ipv4.parse(addr);
      if (ip.isEmpty() || prefix < 0 || prefix > 32) return null;
      return new Cidr(ip.getAsLong() & mask(prefix), prefix);
    }

    public boolean contains(String ip) {
      OptionalLong v = Ipv4.parse(ip);
      return v.isPresent() && (v.getAsLong() & mask(prefix)) == network;
    }

    private static long mask(int prefix) {
      return prefix == 0 ? 0 : (0xFFFFFFFFL << (32 - prefix)) & 0xFFFFFFFFL;
    }
  }
}
