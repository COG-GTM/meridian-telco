package com.meridian.billing.rules;

import com.meridian.billing.calc.CNum;
import com.meridian.billing.model.Account;
import com.meridian.billing.model.UsageRecord;
import java.io.IOException;
import java.io.Reader;
import java.io.UncheckedIOException;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Stream;
import org.apache.commons.csv.CSVFormat;
import org.apache.commons.csv.CSVRecord;
import org.springframework.stereotype.Component;

/**
 * billing/accounts.cpp: the account master (one KEY=VALUE {@code .rec} file per account under
 * billing/accounts/) and the monthly usage file ({@code ACCT_ID,PERIOD,USAGE_MB}).
 */
@Component
public class AccountsRule {

  /** {@code parse_rec}: unknown keys ignored, numbers via atof/atoi, defaults as in the C++. */
  public static Account parseRec(List<String> lines) {
    Map<String, String> kv = new LinkedHashMap<>();
    for (String line : lines) {
      int eq = line.indexOf('=');
      if (eq < 0) continue;
      kv.put(line.substring(0, eq), line.substring(eq + 1));
    }
    return fromFields(kv);
  }

  public static Account fromFields(Map<String, String> kv) {
    return new Account(
        kv.getOrDefault("ACCT_ID", ""),
        kv.getOrDefault("BILLING_REF", ""),
        kv.getOrDefault("CUST_NM", ""),
        kv.getOrDefault("TAX_ID", ""),
        kv.getOrDefault("SVC_ADDR", ""),
        kv.getOrDefault("PROVINCE", ""),
        kv.getOrDefault("PLAN_CD", ""),
        dec(kv, "PLAN_FEE"),
        kv.containsKey("INCLUDED_GB") ? CNum.atol(kv.get("INCLUDED_GB")) : 0,
        dec(kv, "PREV_PLAN_FEE"),
        kv.containsKey("PLAN_CHG_DAY") ? CNum.atoi(kv.get("PLAN_CHG_DAY")) : 0,
        kv.containsKey("LINE_CNT") ? CNum.atoi(kv.get("LINE_CNT")) : 1,
        dec(kv, "PROMO_AMT"),
        kv.getOrDefault("PROMO_DT", ""),
        kv.containsKey("SUSP_START") ? CNum.atoi(kv.get("SUSP_START")) : 0,
        kv.containsKey("SUSP_END") ? CNum.atoi(kv.get("SUSP_END")) : 0,
        dec(kv, "PRIOR_BAL"),
        kv.getOrDefault("PRIOR_DUE", ""),
        dec(kv, "LOYALTY_PCT"));
  }

  private static BigDecimal dec(Map<String, String> kv, String key) {
    return kv.containsKey(key) ? CNum.atof(kv.get(key)) : BigDecimal.ZERO;
  }

  /** {@code load_accounts}: every {@code *.rec} in the directory (sorted by file name). */
  public static List<Account> loadAccounts(Path dir) {
    if (!Files.isDirectory(dir)) return List.of();
    try (Stream<Path> files = Files.list(dir)) {
      List<Path> recs = files
          .filter(p -> {
            String n = p.getFileName().toString();
            return n.length() >= 5 && n.endsWith(".rec");
          })
          .sorted()
          .toList();
      List<Account> out = new ArrayList<>(recs.size());
      for (Path rec : recs) {
        out.add(parseRec(Files.readAllLines(rec, StandardCharsets.UTF_8)));
      }
      return out;
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
  }

  /** {@code load_usage}: header row, columns by name, blank lines skipped, missing cells empty. */
  public static List<UsageRecord> loadUsage(Path csv) {
    if (!Files.isRegularFile(csv)) return List.of();
    CSVFormat format = CSVFormat.DEFAULT.builder()
        .setHeader()
        .setSkipHeaderRecord(true)
        .setIgnoreEmptyLines(true)
        .setAllowMissingColumnNames(true)
        .build();
    List<UsageRecord> out = new ArrayList<>();
    try (Reader in = Files.newBufferedReader(csv, StandardCharsets.UTF_8)) {
      for (CSVRecord r : format.parse(in)) {
        out.add(new UsageRecord(cell(r, "ACCT_ID"), cell(r, "PERIOD"), CNum.atol(cell(r, "USAGE_MB"))));
      }
    } catch (IOException e) {
      throw new UncheckedIOException(e);
    }
    return out;
  }

  private static String cell(CSVRecord r, String col) {
    return r.isMapped(col) && r.isSet(col) ? r.get(col) : "";
  }

  /** {@code find_account}: the first record with that id. */
  public static Map<String, Account> index(Collection<Account> accounts) {
    Map<String, Account> byId = new LinkedHashMap<>();
    for (Account a : accounts) byId.putIfAbsent(a.acctId(), a);
    return byId;
  }

  public Optional<Account> findAccount(Map<String, Account> index, String acctId) {
    return Optional.ofNullable(index.get(acctId));
  }
}
