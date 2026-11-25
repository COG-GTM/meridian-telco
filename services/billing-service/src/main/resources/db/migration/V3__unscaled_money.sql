-- Keep account money exactly as written in the .rec files: a fixed scale would round
-- e.g. PLAN_FEE=1.12499 to 1.1250 before proration and change the billed cent.
ALTER TABLE account
  ALTER COLUMN plan_fee      TYPE NUMERIC,
  ALTER COLUMN prev_plan_fee TYPE NUMERIC,
  ALTER COLUMN promo_amt     TYPE NUMERIC,
  ALTER COLUMN prior_bal     TYPE NUMERIC,
  ALTER COLUMN loyalty_pct   TYPE NUMERIC;
