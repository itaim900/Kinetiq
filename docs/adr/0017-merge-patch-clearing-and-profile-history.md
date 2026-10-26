# ADR-0017: JSON Merge Patch for clearing + an append-only profile history

**Status:** Accepted · **Date:** 2026-10-25 · Supersedes Slice 2 decision D3 (no null-clearing)

## Context

Users need to clear saved profile values. The owner also wants the app to keep using the last
known value (e.g. weight) for training after a field is cleared — and to disclose that.

## Options considered

- **Explicit "clear" list in the body** — non-standard.
- **JSON Merge Patch (RFC 7396)** — absent = unchanged, value = set, `null` = clear.
- For keeping old values: **`last_known_*` columns** (one value, no dates) vs an **append-only
  history table** (every change with a timestamp).

## Decision

`PATCH /users/me` follows JSON Merge Patch; `unit_preference` can't be cleared (422). Every changed
profile field is recorded in `user_profile_history` (old value, new value, time) through
`IProfileHistoryRepository`. Injury `notes: null` also clears.
Details: `docs/superpowers/specs/2026-10-25-themes-units-clearing-design.md`.

## Consequences

- **Positive:** standard, predictable API; nothing is lost when a value is cleared; history enables
  future progress features (e.g. weight over time).
- **Negative:** cleared data is still retained → must be disclosed in the user agreement and fully
  erased by a future "delete my data" (backlog). The history write is not atomic with the profile
  save ([ADR-0015](./0015-autocommit-repository-pattern.md)).
