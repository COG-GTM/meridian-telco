package com.meridian.network.ipam;

/**
 * A {@code target <ip> interval <s> host <name>} line of network/snmp-polling.conf. The poller
 * index is positional (0-based file order), so {@code position} must never be renumbered.
 */
public record SnmpTarget(int position, String ip, Integer intervalSeconds, String host) {}
