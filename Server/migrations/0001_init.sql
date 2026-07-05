-- Migration 0001: baseline
-- Real domain tables arrive in Phase 2. This baseline sets up extensions used
-- by the data model (citext for case-insensitive emails) and a migrations
-- bookkeeping table.

CREATE EXTENSION IF NOT EXISTS citext;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version    TEXT PRIMARY KEY,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

INSERT INTO schema_migrations (version)
VALUES ('0001_init')
ON CONFLICT (version) DO NOTHING;
