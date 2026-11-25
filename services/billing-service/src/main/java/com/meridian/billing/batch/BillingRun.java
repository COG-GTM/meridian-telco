package com.meridian.billing.batch;

import com.meridian.billing.BillingServiceApplication;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import org.springframework.boot.WebApplicationType;
import org.springframework.boot.builder.SpringApplicationBuilder;
import org.springframework.context.ConfigurableApplicationContext;

/**
 * The {@code billing-run} batch entry point, same CLI as the C++ binary:
 * {@code billing-run [--period P] [--json] [--accounts DIR] [--usage CSV]}.
 *
 * <p>By default it reads the flat files, like the legacy binary, and needs no database (this is
 * what vantage-telco parity runs against). With {@code --db} it reads accounts and usage from the
 * billing PostgreSQL store instead and records the run in {@code register_run}/{@code invoice};
 * the usage feed listener is never started by a batch run.
 */
public final class BillingRun {

  public record Options(Path accountsDir, Path usageCsv, String period, boolean json, boolean db) {

    public static Options parse(String... args) {
      Path accounts = Path.of("billing/accounts");
      Path usage = Path.of("data/usage.csv");
      String period = "";
      boolean json = false;
      boolean db = false;
      for (int i = 0; i < args.length; i++) {
        String a = args[i];
        if (a.equals("--accounts") && i + 1 < args.length) accounts = Path.of(args[++i]);
        else if (a.equals("--usage") && i + 1 < args.length) usage = Path.of(args[++i]);
        else if (a.equals("--period") && i + 1 < args.length) period = args[++i];
        else if (a.equals("--json")) json = true;
        else if (a.equals("--db")) db = true;
      }
      return new Options(accounts, usage, period, json, db);
    }
  }

  private BillingRun() {}

  public static void main(String[] args) {
    Options o = Options.parse(args);
    String text;
    if (o.db()) {
      try (ConfigurableApplicationContext ctx = new SpringApplicationBuilder(BillingServiceApplication.class)
          .web(WebApplicationType.NONE)
          .properties("spring.main.banner-mode=off", "spring.main.log-startup-info=false",
              "logging.level.root=WARN")
          .run("--meridian.kafka.enabled=false")) {
        text = ctx.getBean(RegisterJob.class).run(o.period(), o.json()).text();
      }
    } else {
      text = RegisterRenderer.render(o.accountsDir(), o.usageCsv(), o.period(), o.json());
    }
    PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
    out.print(text);
    out.flush();
  }
}
