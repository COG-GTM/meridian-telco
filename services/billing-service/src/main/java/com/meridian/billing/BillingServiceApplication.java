package com.meridian.billing;

import com.meridian.billing.batch.BillingRun;
import java.util.Arrays;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

/**
 * billing-service: /invoices on :8082. {@code java -jar billing-service.jar billing-run [...]}
 * runs the register batch job instead (see {@link BillingRun}).
 */
@SpringBootApplication
public class BillingServiceApplication {

  public static void main(String[] args) {
    if (args.length > 0 && args[0].equals("billing-run")) {
      BillingRun.main(Arrays.copyOfRange(args, 1, args.length));
      return;
    }
    SpringApplication.run(BillingServiceApplication.class, args);
  }
}
