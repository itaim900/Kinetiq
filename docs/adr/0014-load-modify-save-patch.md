# ADR-0014: PATCH = load, modify, validate, save (not dynamic SQL)

**Status:** Accepted · **Date:** 2026-07-17 (recorded 2026-10-25)

## Context

`PATCH /users/me` and injury PATCH update a subset of fields. Two ways: build an `UPDATE … SET`
with only the provided columns, or load the entity, apply the provided fields in C++, validate the
whole result, and save all columns.

## Options considered

- **Dynamic SQL** — touches only changed columns; validation would have to reason about partial data.
- **Load-modify-save** — the domain policy validates a complete entity; repository has one `update`.

## Decision

Load-modify-save in the app service (`UpdateProfile`, `InjuryService::update`), validating the
complete entity with the domain policy before persisting.

## Consequences

- **Positive:** one validation path for create and update; simple repositories; easy fakes.
- **Negative:** last-write-wins between concurrent requests for the same user (accepted for v1);
  load and save are separate statements (see [ADR-0015](./0015-autocommit-repository-pattern.md)).
