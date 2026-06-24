# ADR-0004: Database is PostgreSQL

**Status:** Accepted · **Date:** 2026-06-24

## Context

The data model (`01`) is relational with many-to-many joins, constraints, and JSON fields
(`preferences`, audit payloads). We want a production-realistic, portfolio-worthy choice and to
learn industry-standard SQL tooling.

## Options considered

- **PostgreSQL** — robust, standard, rich types (`JSONB`, arrays, `CITEXT`), strong constraints,
  excellent Docker story. Drogon has a native client.
- **SQLite** — zero-config, simplest for local dev; weaker concurrency and types; less realistic.
- **MySQL/MariaDB** — viable, but fewer of the rich types we use.

## Decision

Use **PostgreSQL**, run locally via Docker.

## Consequences

- **Positive:** uses the exact `JSONB`/array/`CITEXT` features the schema relies on; realistic;
  teaches Docker alongside SQL.
- **Negative:** requires running a container locally (vs. a single SQLite file).
- **Testing:** integration tests run against an ephemeral Postgres container (see test strategy).
