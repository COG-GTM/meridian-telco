package com.meridian.billing.api;

import com.meridian.billing.model.Account;
import com.meridian.billing.model.UsageRecord;
import com.meridian.billing.rules.AccountsRule;
import com.meridian.billing.rules.InvoiceComposer;
import com.meridian.billing.store.AccountRepository;
import com.meridian.billing.store.UsageRepository;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
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
      @RequestParam(name = "acct", required = false) List<String> accts,
      @RequestParam(name = "period", required = false) List<String> periods) {
    String acct = last(accts);
    String period = last(periods);
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

  /** httpd.h parse_target: a repeated query key keeps its last value. */
  private static String last(List<String> values) {
    return values == null || values.isEmpty() ? "" : values.getLast();
  }
}
