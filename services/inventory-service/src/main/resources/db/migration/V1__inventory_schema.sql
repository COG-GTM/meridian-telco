-- inventory-service store. Replaces the SITE and CIRCUIT tables of the shared meridian.db and
-- adds the telemetry drops that were read straight from data/ (locations, usage).

CREATE TABLE site (
  asset_id        INTEGER PRIMARY KEY,
  site_nm         TEXT,
  site_cd         TEXT,
  region_cd       TEXT,
  lat             DOUBLE PRECISION,
  lon             DOUBLE PRECISION,
  status_cd       INTEGER,
  tower_reg       TEXT,
  total_cap_mbps  INTEGER,
  alloc_cap_mbps  INTEGER
);
CREATE INDEX site_region_cd ON site (region_cd);
CREATE INDEX site_status_cd ON site (status_cd);

CREATE TABLE circuit (
  circuit_id  TEXT PRIMARY KEY,
  circuit_nm  TEXT,
  a_asset_id  INTEGER,
  z_asset_id  INTEGER,
  cap_mbps    INTEGER,
  alloc_mbps  INTEGER,
  role_cd     INTEGER,
  status_cd   INTEGER
);

-- serviceable customer locations (data/locations.csv), file order preserved by seq
CREATE TABLE location (
  loc_cd          TEXT PRIMARY KEY,
  seq             INTEGER NOT NULL,
  cust_nm         TEXT,
  loc_nm          TEXT,
  market_cd       TEXT,
  total_cap_mbps  INTEGER,
  alloc_cap_mbps  INTEGER
);

-- monthly usage per account (data/usage.csv); published to billing as UsageRecorded
CREATE TABLE usage_record (
  acct_id      TEXT NOT NULL,
  period       TEXT NOT NULL,
  seq          INTEGER NOT NULL,
  usage_mb     BIGINT NOT NULL,
  source       TEXT,
  recorded_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at TIMESTAMPTZ,
  PRIMARY KEY (acct_id, period)
);

CREATE TABLE ingest_run (
  id               BIGSERIAL PRIMARY KEY,
  data_dir         TEXT NOT NULL,
  started_at       TIMESTAMPTZ NOT NULL,
  finished_at      TIMESTAMPTZ,
  sites_loaded     INTEGER,
  circuits_loaded  INTEGER,
  locations_loaded INTEGER,
  usage_loaded     INTEGER,
  report           TEXT
);
