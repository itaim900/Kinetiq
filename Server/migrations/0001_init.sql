-- Migration 0001: baseline extensions.
--
-- The schema_migrations bookkeeping table is created and owned by the app's
-- startup Migrator (see Server/src/adapters/db/Migrator.*), not here. This file
-- only enables extensions the data model relies on. Real domain tables arrive
-- in 0002_core_schema.sql.

CREATE EXTENSION IF NOT EXISTS citext;
