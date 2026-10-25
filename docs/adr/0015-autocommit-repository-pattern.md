# ADR-0015: Two-layer repositories (Pg… + AutoCommit…)

**Status:** Accepted · **Date:** 2026-07-16 (recorded 2026-10-25)

## Context

Crow routes capture their dependencies once at startup and serve concurrent requests on many
threads. A `pqxx::work` (transaction) is per-operation and must not be shared across threads.
Integration tests, however, want one transaction per test that is rolled back afterwards.

## Options considered

- **One shared connection/transaction** — unsafe across concurrent requests.
- **Thread a transaction through each request** — correct atomicity, but nothing in Crow supports it
  and every service signature would change.
- **Two layers** — `PgXRepository(pqxx::work&)` for SQL; `AutoCommitPgXRepository(connString)`
  opens a connection + transaction per call and delegates.

## Decision

Every repository has both layers. Tests use `PgX` inside `DbTest`'s rolled-back transaction;
`main.cpp` wires `AutoCommitPgX`.

## Consequences

- **Positive:** thread-safe in production; isolated, fast DB tests; same SQL in both.
- **Negative:** multi-step use cases (e.g. profile save + history write) are not one atomic
  transaction — a crash between steps can leave them inconsistent. Accepted for v1; revisit with a
  unit-of-work if it ever matters.
