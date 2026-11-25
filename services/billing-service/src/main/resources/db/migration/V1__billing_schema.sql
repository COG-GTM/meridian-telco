-- billing-service store. Account master (billing/accounts/*.rec), usage received from
-- inventory-service, and the invoices/registers that were previously only printed.
-- Money is NUMERIC, never floating point.

CREATE TABLE account (
  acct_id        TEXT PRIMARY KEY,
  billing_ref    TEXT,
  cust_nm        TEXT,
  tax_id         TEXT,
  svc_addr       TEXT,
  province       TEXT,
  plan_cd        TEXT,
  plan_fee       NUMERIC(14,4) NOT NULL DEFAULT 0,
  included_gb    BIGINT NOT NULL DEFAULT 0,
  prev_plan_fee  NUMERIC(14,4) NOT NULL DEFAULT 0,
  plan_chg_day   INTEGER NOT NULL DEFAULT 0,
  line_cnt       INTEGER NOT NULL DEFAULT 1,
  promo_amt      NUMERIC(14,4) NOT NULL DEFAULT 0,
  promo_dt       TEXT,
  susp_start     INTEGER NOT NULL DEFAULT 0,
  susp_end       INTEGER NOT NULL DEFAULT 0,
  prior_bal      NUMERIC(14,4) NOT NULL DEFAULT 0,
  prior_due      TEXT,
  loyalty_pct    NUMERIC(8,4) NOT NULL DEFAULT 0,
  source_file    TEXT
);

-- consumed from meridian.usage.v1; seq keeps the legacy usage.csv order for the register
CREATE TABLE usage_record (
  acct_id      TEXT NOT NULL,
  period       TEXT NOT NULL,
  seq          INTEGER NOT NULL,
  usage_mb     BIGINT NOT NULL,
  source       TEXT,
  received_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (acct_id, period)
);

CREATE TABLE register_run (
  id             BIGSERIAL PRIMARY KEY,
  period         TEXT,
  started_at     TIMESTAMPTZ NOT NULL,
  finished_at    TIMESTAMPTZ,
  invoice_count  INTEGER,
  grand_total    NUMERIC(16,2)
);

CREATE TABLE invoice (
  register_run_id    BIGINT NOT NULL REFERENCES register_run (id),
  acct_id            TEXT NOT NULL,
  period             TEXT NOT NULL,
  billing_ref        TEXT,
  province           TEXT,
  usage_mb           BIGINT,
  usage_gb_rated     BIGINT,
  overage_gb         BIGINT,
  plan_charge        NUMERIC(14,2),
  line_discount      NUMERIC(14,2),
  recurring          NUMERIC(14,2),
  overage_charges    NUMERIC(14,2),
  suspension_credit  NUMERIC(14,2),
  promo_credit       NUMERIC(14,2),
  late_fee           NUMERIC(14,2),
  subtotal           NUMERIC(14,2),
  loyalty            NUMERIC(14,2),
  federal_tax        NUMERIC(14,2),
  provincial_tax     NUMERIC(14,2),
  federal_label      TEXT,
  provincial_label   TEXT,
  total              NUMERIC(14,2),
  PRIMARY KEY (register_run_id, acct_id, period)
);
