-- usage_record is fed by meridian.usage.v1: seq comes from a sequence (arrival order), and
-- recorded_at (the producer's timestamp) lets the upsert ignore out-of-order replays.
CREATE SEQUENCE usage_record_seq;
ALTER TABLE usage_record ALTER COLUMN seq SET DEFAULT nextval('usage_record_seq');
ALTER TABLE usage_record ADD COLUMN recorded_at TIMESTAMPTZ;
