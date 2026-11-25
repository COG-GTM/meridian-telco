package com.meridian.billing.batch;

import com.meridian.billing.batch.RegisterRenderer.Register;
import com.meridian.billing.rules.InvoiceComposer;
import com.meridian.billing.store.AccountRepository;
import com.meridian.billing.store.RegisterRepository;
import com.meridian.billing.store.UsageRepository;
import java.time.Instant;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/** billing-run against the billing store: renders the register and records the run and invoices. */
@Service
public class RegisterJob {

  private final InvoiceComposer composer;
  private final AccountRepository accounts;
  private final UsageRepository usage;
  private final RegisterRepository registers;

  public RegisterJob(InvoiceComposer composer, AccountRepository accounts, UsageRepository usage,
      RegisterRepository registers) {
    this.composer = composer;
    this.accounts = accounts;
    this.usage = usage;
    this.registers = registers;
  }

  public record Result(long registerRunId, String text, Register register) {}

  @Transactional
  public Result run(String period, boolean json) {
    Instant started = Instant.now();
    String p = period == null || period.isEmpty() ? null : period;
    Register reg = RegisterRenderer.render(composer, accounts.findAll(), usage.findAll(), p, json);
    long id = registers.save(p, started, Instant.now(), reg.invoices(), reg.grandTotal());
    return new Result(id, reg.text(), reg);
  }
}
