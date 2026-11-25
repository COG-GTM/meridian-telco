package com.meridian.billing.batch;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/**
 * Freezes the billing-run register and the --json reconciliation feed that vantage-telco's
 * tools/parity consumes. Output must be byte-identical to contracts/register/* captured from
 * the C++ bin/billing-run (400 invoices: 200 accounts x 2026-06, 2026-07).
 */
class RegisterFeedGoldenTest {

  static final Path ACCOUNTS = MeridianRoot.resolve("billing/accounts");
  static final Path USAGE = MeridianRoot.resolve("data/usage.csv");

  @ParameterizedTest(name = "{0}")
  @CsvSource(nullValues = "ALL", value = {
      "register-2026-06.jsonl, 2026-06, true",
      "register-2026-07.jsonl, 2026-07, true",
      "register-all.jsonl,     ALL,     true",
      "register-2026-06.txt,   2026-06, false",
      "register-2026-07.txt,   2026-07, false",
      "register-all.txt,       ALL,     false"})
  void registerMatchesLegacyByteForByte(String golden, String period, boolean json) throws IOException {
    String want = Files.readString(MeridianRoot.resolve("contracts/register/" + golden));
    String got = RegisterRenderer.render(ACCOUNTS, USAGE, period, json);
    if (!want.equals(got)) {
      String[] w = want.split("\n", -1);
      String[] g = got.split("\n", -1);
      for (int i = 0; i < Math.min(w.length, g.length); i++) {
        assertThat(g[i]).as("%s line %d", golden, i + 1).isEqualTo(w[i]);
      }
      assertThat(g.length).as("%s line count", golden).isEqualTo(w.length);
    }
  }
}
