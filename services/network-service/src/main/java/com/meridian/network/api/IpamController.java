package com.meridian.network.api;

import com.meridian.network.ipam.ConfigAnalysis;
import com.meridian.network.ipam.Device;
import com.meridian.network.ipam.IpamReport;
import com.meridian.network.ipam.ReservedRange;
import com.meridian.network.ipam.SnmpTarget;
import com.meridian.network.store.NetworkLoader;
import com.meridian.network.store.NetworkStore;
import java.util.List;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.ResponseStatus;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.server.ResponseStatusException;

@RestController
public class IpamController {

  /**
   * {@code legacy}: the figures bin/ipam prints (hardcoded 10.20.250.0/24 reserved range).
   * {@code config}: reserved-ranges.conf and snmp-polling.conf cross-checked against the devices.
   */
  public record IpamReportView(IpamReport.Summary legacy, ConfigAnalysis config) {}

  public record DeviceList(int count, List<Device> devices) {}

  public record ReservedRangeList(int count, List<ReservedRange> reservedRanges) {}

  public record SnmpTargetList(int count, List<SnmpTarget> snmpTargets) {}

  public record ErrorBody(String error) {}

  private final NetworkStore store;
  private final NetworkLoader loader;

  public IpamController(NetworkStore store, NetworkLoader loader) {
    this.store = store;
    this.loader = loader;
  }

  @GetMapping("/ipam/report")
  public IpamReportView report() {
    List<Device> devices = loadedDevices();
    return new IpamReportView(IpamReport.summarize(devices),
        ConfigAnalysis.of(devices, store.reservedRanges(), store.snmpTargets()));
  }

  /** Byte-identical to bin/ipam stdout over the loaded devices.csv. */
  @GetMapping(value = "/ipam/report.txt", produces = "text/plain;charset=UTF-8")
  public String reportText() {
    return IpamReport.legacyText(loadedDevices());
  }

  @PostMapping("/ipam/refresh")
  public NetworkLoader.LoadResult refresh() {
    return loader.refresh();
  }

  @GetMapping("/devices")
  public DeviceList devices() {
    List<Device> d = store.devices();
    return new DeviceList(d.size(), d);
  }

  @GetMapping("/reserved-ranges")
  public ReservedRangeList reservedRanges() {
    List<ReservedRange> r = store.reservedRanges();
    return new ReservedRangeList(r.size(), r);
  }

  @GetMapping("/snmp-targets")
  public SnmpTargetList snmpTargets() {
    List<SnmpTarget> t = store.snmpTargets();
    return new SnmpTargetList(t.size(), t);
  }

  @ExceptionHandler(IllegalStateException.class)
  @ResponseStatus(HttpStatus.SERVICE_UNAVAILABLE)
  public ErrorBody unavailable(IllegalStateException e) {
    return new ErrorBody(e.getMessage());
  }

  private List<Device> loadedDevices() {
    List<Device> devices = store.devices();
    if (devices.isEmpty()) {
      throw new ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "no devices loaded");
    }
    return devices;
  }
}
