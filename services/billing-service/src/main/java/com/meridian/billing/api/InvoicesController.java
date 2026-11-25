package com.meridian.billing.api;

import com.meridian.billing.model.Account;
import com.meridian.billing.model.UsageRecord;
import com.meridian.billing.rules.AccountsRule;
import com.meridian.billing.rules.InvoiceComposer;
import com.meridian.billing.store.AccountRepository;
import com.meridian.billing.store.UsageRepository;
import jakarta.servlet.http.HttpServletRequest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

/**
 * {@code GET /invoices?acct=&period=} (billing/invoice-api/main.cpp). Every usage record with an
 * account master, in register order; 404 with an empty list when an account filter matches
 * nothing. The legacy router matched on path prefix, any method and any Accept, so this does too.
 */
@RestController
public class InvoicesController {

  private final InvoiceComposer composer;
  private final AccountRepository accounts;
  private final UsageRepository usage;

  public InvoicesController(InvoiceComposer composer, AccountRepository accounts, UsageRepository usage) {
    this.composer = composer;
    this.accounts = accounts;
    this.usage = usage;
  }

  @RequestMapping(path = {"/invoices", "/invoices*", "/invoices/**", "/invoices*/**"})
  public ResponseEntity<InvoiceView.Page> invoices(
      HttpServletRequest req) {
    Map<String, String> q = parseQuery(req.getQueryString());
    String acct = q.getOrDefault("acct", "");
    String period = q.getOrDefault("period", "");
    Map<String, Account> byId = AccountsRule.index(accounts.findAll());
    List<InvoiceView> out = new ArrayList<>();
    for (UsageRecord u : usage.findAll()) {
      if (!acct.isEmpty() && !u.acctId().equals(acct)) continue;
      if (!period.isEmpty() && !u.period().equals(period)) continue;
      Account a = byId.get(u.acctId());
      if (a == null) continue;
      out.add(InvoiceView.of(composer.compose(a, u), a));
    }
    HttpStatus status = out.isEmpty() && !acct.isEmpty() ? HttpStatus.NOT_FOUND : HttpStatus.OK;
    return ResponseEntity.status(status).contentType(MediaType.APPLICATION_JSON).body(new InvoiceView.Page(out));
  }

  /**
   * httpd.h parse_target on the raw query string: split on '&' then the first '=', no
   * percent-decoding, a repeated key keeps its last value, and the body is never read.
   */
  static Map<String, String> parseQuery(String qs) {
    Map<String, String> out = new HashMap<>();
    if (qs == null) return out;
    for (String kv : qs.split("&", -1)) {
      int eq = kv.indexOf('=');
      if (eq >= 0) out.put(kv.substring(0, eq), kv.substring(eq + 1));
    }
    return out;
  }
}
