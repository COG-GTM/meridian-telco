-- network-service store, loaded from the network/ config files.

CREATE TABLE device (
  device_name   TEXT NOT NULL,
  seq           INTEGER PRIMARY KEY,
  region        TEXT,
  role          TEXT,
  mgmt_ip       TEXT NOT NULL,
  site_cd       TEXT,
  external_bgp  BOOLEAN NOT NULL DEFAULT FALSE
);

CREATE TABLE reserved_range (
  cidr     TEXT PRIMARY KEY,
  purpose  TEXT
);

-- snmp-polling.conf: "do not reorder, index is positional"
CREATE TABLE snmp_target (
  position          INTEGER PRIMARY KEY,
  ip                TEXT NOT NULL,
  interval_seconds  INTEGER,
  host              TEXT
);
