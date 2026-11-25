package com.meridian.billing.batch;

import static org.assertj.core.api.Assertions.assertThat;

import com.meridian.contracts.testkit.MeridianRoot;
import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/** The billing-run CLI: same flags as the C++ binary, same bytes on stdout. */
class BillingRunTest {

  @ParameterizedTest(name = "{0}")
  @CsvSource({
      "register-2026-07.jsonl, --period 2026-07 --json",
      "register-2026-06.txt,   --period 2026-06",
      "register-all.jsonl,     --json"})
  void printsTheFrozenRegister(String golden, String flags) throws Exception {
    String[] args = (flags + " --accounts " + MeridianRoot.resolve("billing/accounts")
        + " --usage " + MeridianRoot.resolve("data/usage.csv")).trim().split("\\s+");
    PrintStream saved = System.out;
    ByteArrayOutputStream buf = new ByteArrayOutputStream();
    System.setOut(new PrintStream(buf, true, StandardCharsets.UTF_8));
    try {
      BillingRun.main(args);
    } finally {
      System.setOut(saved);
    }
    assertThat(buf.toString(StandardCharsets.UTF_8))
        .isEqualTo(Files.readString(MeridianRoot.resolve("contracts/register/" + golden)));
  }

  @Test
  void parsesLegacyFlagsAndDefaults() {
    var o = BillingRun.Options.parse();
    assertThat(o).isEqualTo(new BillingRun.Options(Path.of("billing/accounts"), Path.of("data/usage.csv"), "", false, false));
    o = BillingRun.Options.parse("--json", "--period", "2026-06", "--usage", "u.csv", "--accounts", "a", "--db");
    assertThat(o).isEqualTo(new BillingRun.Options(Path.of("a"), Path.of("u.csv"), "2026-06", true, true));
  }

  @Test
  void unknownAccountsPrintNoAccountRowsInTextOnly(@org.junit.jupiter.api.io.TempDir Path tmp) throws Exception {
    Path usage = tmp.resolve("usage.csv");
    Files.writeString(usage, "ACCT_ID,PERIOD,USAGE_MB\nGHOST-1,2026-06,12\n");
    Path accounts = MeridianRoot.resolve("billing/accounts");
    assertThat(RegisterRenderer.render(accounts, usage, "2026-06", true)).isEmpty();
    assertThat(RegisterRenderer.render(accounts, usage, "2026-06", false)).contains(
        "GHOST-1          2026-06  -              12          -            -          -   NO ACCOUNT\n");
  }
}
