package com.meridian.network.ipam;

/** One row of network/devices.csv; {@code seq} is the 1-based file order. */
public record Device(int seq, String deviceName, String region, String role, String mgmtIp,
    String siteCd, boolean externalBgp) {}
