package com.meridian.network.store;

import com.meridian.network.ipam.Device;
import com.meridian.network.ipam.Ipv4;
import com.meridian.network.ipam.ReservedRange;
import com.meridian.network.ipam.SnmpTarget;
import java.util.Comparator;
import java.util.List;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Repository;
import org.springframework.transaction.annotation.Transactional;

/** network-service's own PostgreSQL store (Flyway V1: device, reserved_range, snmp_target). */
@Repository
public class NetworkStore {

  private static final Comparator<ReservedRange> BY_ADDRESS = Comparator
      .comparingLong((ReservedRange r) -> {
        Ipv4.Cidr c = Ipv4.Cidr.parse(r.cidr());
        return c == null ? Long.MAX_VALUE : c.network();
      })
      .thenComparing(ReservedRange::cidr);

  private final JdbcTemplate jdbc;

  public NetworkStore(JdbcTemplate jdbc) {
    this.jdbc = jdbc;
  }

  @Transactional
  public void replaceAll(List<Device> devices, List<ReservedRange> ranges, List<SnmpTarget> targets) {
    jdbc.execute("TRUNCATE device, reserved_range, snmp_target");
    jdbc.batchUpdate("INSERT INTO device (seq, device_name, region, role, mgmt_ip, site_cd, external_bgp)"
        + " VALUES (?,?,?,?,?,?,?)", devices, 500, (ps, d) -> {
          ps.setInt(1, d.seq());
          ps.setString(2, d.deviceName());
          ps.setString(3, d.region());
          ps.setString(4, d.role());
          ps.setString(5, d.mgmtIp());
          ps.setString(6, d.siteCd());
          ps.setBoolean(7, d.externalBgp());
        });
    jdbc.batchUpdate("INSERT INTO reserved_range (cidr, purpose) VALUES (?,?)", ranges, 500, (ps, r) -> {
      ps.setString(1, r.cidr());
      ps.setString(2, r.purpose());
    });
    jdbc.batchUpdate("INSERT INTO snmp_target (position, ip, interval_seconds, host) VALUES (?,?,?,?)",
        targets, 500, (ps, t) -> {
          ps.setInt(1, t.position());
          ps.setString(2, t.ip());
          ps.setObject(3, t.intervalSeconds());
          ps.setString(4, t.host());
        });
  }

  public List<Device> devices() {
    return jdbc.query("SELECT seq, device_name, region, role, mgmt_ip, site_cd, external_bgp FROM device ORDER BY seq",
        (rs, i) -> new Device(rs.getInt(1), rs.getString(2), rs.getString(3), rs.getString(4),
            rs.getString(5), rs.getString(6), rs.getBoolean(7)));
  }

  /** The table has no order column; ranges come back in address order. */
  public List<ReservedRange> reservedRanges() {
    return jdbc.query("SELECT cidr, purpose FROM reserved_range",
        (rs, i) -> new ReservedRange(rs.getString(1), rs.getString(2)))
        .stream().sorted(BY_ADDRESS).toList();
  }

  public List<SnmpTarget> snmpTargets() {
    return jdbc.query("SELECT position, ip, interval_seconds, host FROM snmp_target ORDER BY position",
        (rs, i) -> new SnmpTarget(rs.getInt(1), rs.getString(2),
            rs.getObject(3) == null ? null : rs.getInt(3), rs.getString(4)));
  }
}
