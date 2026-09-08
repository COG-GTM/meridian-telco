# Shared telco billing rules

This directory is the single source of truth for the billing rules shared by
meridian-telco and vantage-telco. The policy is defined in
`telco_billing_rules/billing_rules.json`; rules change here, never in a
consumer.

The Python package can be installed from git by vantage-telco:

```text
telco-billing-rules @ git+https://github.com/COG-GTM/meridian-telco.git@<commit>#subdirectory=billing-rules
```

For local development, install it with `pip install -e
../meridian-telco/billing-rules`. The meridian C++ billing engine includes
`cpp/telco_billing_rules.h` directly through its Makefile include path.

## Rules

- Billing proration uses a fixed 30-day month.
- Usage is rounded up to whole gigabytes, then overage is charged at $10/GB.
- Federal GST/HST is assessed on the pre-loyalty-discount amount.
- Provincial PST/QST is assessed after the loyalty discount.
- Every charge line is rounded to cents using half-up rounding.

The JSON file inside the package is canonical. The C++ header mirrors its
constants and province table, and `tests/test_rules.py` checks that they do not
drift.
