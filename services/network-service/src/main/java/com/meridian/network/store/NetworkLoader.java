package com.meridian.network.store;

import com.meridian.network.ipam.Device;
import com.meridian.network.ipam.NetworkFiles;
import com.meridian.network.ipam.ReservedRange;
import com.meridian.network.ipam.SnmpTarget;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.stereotype.Component;

/** Loads network/ (devices.csv, reserved-ranges.conf, snmp-polling.conf) into the store. */
@Component
public class NetworkLoader implements ApplicationRunner {

  public record LoadResult(String networkDir, int devices, int reservedRanges, int snmpTargets) {}

  private static final Logger log = LoggerFactory.getLogger(NetworkLoader.class);

  private final NetworkStore store;
  private final Path networkDir;
  private final boolean loadOnStartup;

  public NetworkLoader(NetworkStore store,
      @Value("${meridian.network-dir}") Path networkDir,
      @Value("${meridian.network.load-on-startup:true}") boolean loadOnStartup) {
    this.store = store;
    this.networkDir = networkDir;
    this.loadOnStartup = loadOnStartup;
  }

  @Override
  public void run(ApplicationArguments args) {
    if (!loadOnStartup) return;
    String missing = missingFile();
    if (missing != null) {
      log.warn("no {} in {}; store left as is (POST /ipam/refresh to load)", missing,
          networkDir.toAbsolutePath());
      return;
    }
    LoadResult r = refresh();
    log.info("loaded {}", r);
  }

  /**
   * Replaces the store contents with the current files. Refuses (store untouched) when any of the
   * three files is missing or devices.csv has no rows.
   */
  public synchronized LoadResult refresh() {
    String missing = missingFile();
    if (missing != null) throw new IllegalStateException("no " + missing + " in network dir");
    List<Device> devices = NetworkFiles.readDevices(networkDir.resolve(NetworkFiles.DEVICES));
    if (devices.isEmpty()) throw new IllegalStateException("no devices in " + NetworkFiles.DEVICES);
    List<ReservedRange> ranges = NetworkFiles.readReservedRanges(networkDir.resolve(NetworkFiles.RESERVED_RANGES));
    List<SnmpTarget> targets = NetworkFiles.readSnmpTargets(networkDir.resolve(NetworkFiles.SNMP_POLLING));
    store.replaceAll(devices, ranges, targets);
    return new LoadResult(networkDir.toString(), devices.size(), ranges.size(), targets.size());
  }

  private String missingFile() {
    for (String f : List.of(NetworkFiles.DEVICES, NetworkFiles.RESERVED_RANGES, NetworkFiles.SNMP_POLLING)) {
      if (!Files.isRegularFile(networkDir.resolve(f))) return f;
    }
    return null;
  }
}
