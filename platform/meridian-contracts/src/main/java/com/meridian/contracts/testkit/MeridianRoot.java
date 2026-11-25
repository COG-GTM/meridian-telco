package com.meridian.contracts.testkit;

import java.nio.file.Files;
import java.nio.file.Path;

/** Locates the repository root (where contracts/, data/, billing/ and network/ live). */
public final class MeridianRoot {

  private MeridianRoot() {}

  public static Path path() {
    String prop = System.getProperty("meridian.root");
    if (prop != null && !prop.isBlank()) {
      return Path.of(prop).toAbsolutePath().normalize();
    }
    Path dir = Path.of("").toAbsolutePath();
    while (dir != null) {
      if (Files.isDirectory(dir.resolve("contracts")) && Files.exists(dir.resolve("pom.xml"))) {
        return dir;
      }
      dir = dir.getParent();
    }
    throw new IllegalStateException("cannot locate the meridian repository root; set -Dmeridian.root");
  }

  public static Path resolve(String relative) {
    return path().resolve(relative);
  }
}
