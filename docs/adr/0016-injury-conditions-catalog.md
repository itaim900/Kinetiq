# ADR-0016: Injuries via a conditions catalog (known = locked; unknown = user-described)

**Status:** Accepted · **Date:** 2026-10-21 (recorded 2026-10-25) · Supersedes the single
`muscle_id` design (spec 2026-07-25, decisions E2/E4/E5)

## Context

Injuries should reflect their real effect on the body: several muscles, with a role. No open
database maps injuries → affected muscles (SNOMED CT only gives the primary finding site; ICD-11
gives codes, no muscles).

## Options considered

- **User picks muscles freely** — simple, but accuracy depends on the user's anatomy knowledge.
- **Catalog only** — accurate for known conditions, no way to describe anything else.
- **Catalog + unknown fallback** — known conditions are locked; unknown injuries are described
  from fixed lists.

## Decision

`injury_conditions` (17, ICD-11-anchored) mapped to muscles with `primary|secondary` roles.
A known injury references its condition and reads muscles from the catalog (fixing the catalog
fixes every user's injury). Unknown injuries store user-picked muscles in `user_injury_muscles`.
Body region is a fixed list (FK) shared with exercise contraindications.
Details: `docs/superpowers/specs/2026-10-21-injury-conditions-catalog-design.md`.

## Consequences

- **Positive:** trustworthy data for the Phase 3 safety filter; one place to correct medical content.
- **Negative:** the catalog is project-authored (secondary muscles are judgement) and needs physio
  review; the UI must show a not-medical-advice notice.
