# ADR-0013: Authentication is a plain function, not Crow middleware

**Status:** Accepted · **Date:** 2026-07-16 (recorded 2026-10-25)

## Context

Protected routes need the current user from `Authorization: Bearer <jwt>`. HTTP tests drive Crow
in-process via `app.handle_full()` (no socket) — and `handle_full()` does **not** run middleware
(global or route-local); a route declaring middleware even dereferences an uninitialised context.

## Options considered

- **Crow middleware** — idiomatic, but untestable in-process; would force socket-based tests.
- **Socket-based HTTP tests** — real middleware, but slow and flaky (ports, startup).
- **A plain function handlers call** — `authenticate(req, tokens) -> optional<CurrentUser>`.

## Decision

`http::authenticate()` in `http/middleware/Authenticator.h`; every protected handler calls it first
and returns 401 on `nullopt`. No Crow middleware on routes.

## Consequences

- **Positive:** the whole request path (routing → auth → service → DTO) is tested in-process, fast.
- **Negative:** each handler must remember the call (one line); a forgotten call isn't caught by
  the framework — every protected route has a 401 test to compensate.
