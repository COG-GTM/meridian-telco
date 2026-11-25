package com.meridian.billing.store;

import com.meridian.billing.model.UsageRecord;
import com.meridian.billing.rules.AccountsRule;
import java.nio.file.Path;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.core.annotation.Order;
import org.springframework.stereotype.Component;

/**
 * Optional bootstrap from the legacy flat files, for environments without db-migration or the
 * usage feed: accounts are loaded only into an empty table, usage never overrides feed records.
 */
@Component
@Order(0)
public class SeedLoader implements ApplicationRunner {

  private static final Logger log = LoggerFactory.getLogger(SeedLoader.class);

  private final AccountRepository accounts;
  private final UsageRepository usage;
  private final String accountsDir;
  private final String usageCsv;

  public SeedLoader(AccountRepository accounts, UsageRepository usage,
      @Value("${meridian.billing.seed.accounts-dir:}") String accountsDir,
      @Value("${meridian.billing.seed.usage-csv:}") String usageCsv) {
    this.accounts = accounts;
    this.usage = usage;
    this.accountsDir = accountsDir;
    this.usageCsv = usageCsv;
  }

  @Override
  public void run(ApplicationArguments args) {
    seed();
  }

  public synchronized void seed() {
    if (!accountsDir.isBlank() && accounts.count() == 0) {
      var loaded = AccountsRule.loadAccounts(Path.of(accountsDir));
      accounts.insertAll(loaded, accountsDir);
      log.info("seeded {} accounts from {}", loaded.size(), accountsDir);
    }
    if (!usageCsv.isBlank()) {
      int n = 0;
      for (UsageRecord u : AccountsRule.loadUsage(Path.of(usageCsv))) {
        n += usage.seed(u, "file:" + Path.of(usageCsv).getFileName());
      }
      log.info("seeded {} usage records from {}", n, usageCsv);
    }
  }
}
