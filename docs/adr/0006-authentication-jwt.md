# ADR-0006: Authentication uses short-lived JWT (v1)

**Status:** Accepted · **Date:** 2026-06-24

## Context

The API needs authenticated requests. The project values learning auth fundamentals and keeping
v1 simple, while not painting us into a corner.

## Options considered

- **Short-lived JWT only** — stateless; learn signing/claims/verification; re-login on expiry.
- **JWT + refresh tokens** — better UX/security; more moving parts (refresh endpoint, rotation).
- **Server-side sessions** — easy revocation; stateful (needs a store); CSRF handling.
- **Managed auth (Auth0/Clerk/etc.)** — least learning; awkward to integrate with C++.

## Decision

Use **short-lived JWT only** for v1. Build it ourselves. JWT verification lives in a single auth
middleware that yields a `CurrentUser`; **no other code decodes tokens**.

## Consequences

- **Positive:** simplest stateless start; teaches JWT internals.
- **Negative:** worse UX on expiry (no silent refresh yet).
- **Future-proofing:** because identity is resolved in one adapter (ADR-0002), switching to refresh
  tokens or server-side sessions later changes that adapter + login/logout + frontend storage only.
