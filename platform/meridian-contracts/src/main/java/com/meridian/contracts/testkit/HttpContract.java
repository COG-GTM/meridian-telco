package com.meridian.contracts.testkit;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.stream.Stream;

/**
 * One frozen HTTP exchange captured from the legacy C++ binaries by
 * {@code tools/legacy/capture-contracts.sh}: {@code contracts/http/<name>.request|.status|.json}.
 */
public record HttpContract(String name, String requestPath, int status, String body) {

  public static HttpContract load(String name) {
    Path dir = MeridianRoot.resolve("contracts/http");
    try {
      return new HttpContract(
          name,
          Files.readString(dir.resolve(name + ".request"), StandardCharsets.UTF_8).strip(),
          Integer.parseInt(Files.readString(dir.resolve(name + ".status")).strip()),
          Files.readString(dir.resolve(name + ".json"), StandardCharsets.UTF_8));
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  /** All frozen exchanges whose name starts with one of the prefixes, e.g. "sites", "invoices". */
  public static List<HttpContract> withPrefix(String... prefixes) {
    try (Stream<Path> files = Files.list(MeridianRoot.resolve("contracts/http"))) {
      return files
          .map(p -> p.getFileName().toString())
          .filter(n -> n.endsWith(".request"))
          .map(n -> n.substring(0, n.length() - ".request".length()))
          .filter(n -> Stream.of(prefixes).anyMatch(n::startsWith))
          .sorted()
          .map(HttpContract::load)
          .toList();
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  @Override
  public String toString() {
    return name + " " + requestPath;
  }
}
