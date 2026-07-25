# Phase 2, Slice 2 — Profile & Injuries: Step-by-Step Build Log

**Status:** 🚧 IN PROGRESS (started 2026-07-25). Stage A (`#19`) ✅ + Stage B (`#20`) ✅ done. A
Stage B extension (injury/muscle link) is designed + planned but not yet executed. Stage C (`#25`)
pending.
**Issues:** #19 (`GET`/`PATCH /users/me`), #20 (injuries CRUD), #25 (React profile + injuries forms)
**Design spec:** [`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`](./superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md) — the *what & why* (8 locked decisions)
**Stage A plan:** [`docs/superpowers/plans/2026-07-17-slice-2a-profile-backend.md`](./superpowers/plans/2026-07-17-slice-2a-profile-backend.md) — the bite-sized TDD steps
**Injury/muscle link:** [design spec](./superpowers/specs/2026-07-25-injury-muscle-link-design.md) (E1–E6) · [implementation plan](./superpowers/plans/2026-07-25-injury-muscle-link.md) (8 tasks, not started)
**Contract:** [`docs/02-api-contract.md`](./02-api-contract.md) — Profile & Injuries sections
**Precedent:** [`docs/phase-2-slice-1-auth.md`](./phase-2-slice-1-auth.md) — the patterns this slice reuses
**Branch:** `slice-2-profile` (all Slice 2 work; `main` untouched until the slice is done).
This is a **living document** — update it as the slice progresses.

---

## Progress checklist

### Stage A — Profile (`#19`) ✅ DONE (2026-07-25)
- [x] Design brainstormed, 8 decisions locked, spec written — 2026-07-17
- [x] Stage A TDD plan written — 2026-07-17
- [x] A1: `domain::User` grown + `domain::ProfilePolicy` validator (12 tests) — `f88f45f`
- [x] A2: `PgUserRepository` maps/persists profile columns + fake round-trip (21 repo tests) — `09531d5`
- [x] A3: `app::UpdateProfile` (load-modify-save, guard-validate) (3 tests) — `cd86e58`
- [x] A4: `http/dto/ProfileDto.h` + `http/controllers/UserController` GET/PATCH (6 HTTP tests) — `e5b56d0`
- [x] A5: wired into `main.cpp`, live-verified via curl — `15eb144`

### Stage B — Injuries (`#20`) ✅ DONE (2026-07-25)
- [x] Stage B TDD plan written — `docs/superpowers/plans/2026-07-25-slice-2b-injuries-backend.md`
- [x] B1: `domain::Injury` + `InjuryPolicy` validation (5 tests) — `c86a624`
- [x] B2: `ports::IInjuryRepository` + `InMemoryInjuryRepository` fake (5 tests) — `013cb30`
- [x] B3: `db::PgInjuryRepository` + `AutoCommitPgInjuryRepository` (10 tests) — `f864966`
- [x] B4: `app::InjuryService` (add/list/update/remove + ownership 404) (6 tests) — `af57de7`
- [x] B5: `http/dto/InjuryDto.h` + `InjuryController` (GET/POST/PATCH/DELETE, 7 HTTP tests) — `e651976`
- [x] B6: wired into `main.cpp`, live-verified — `85dfcb5`

### Stage B extension — Injury/Muscle link ⬜ PLANNED, NOT STARTED (2026-07-25)
Owner requested injuries be able to reference the `Muscles` catalog (e.g. a hamstring injury →
the "Hamstrings" muscle), while correctly staying optional for non-muscle injuries (bone, tendon,
joint). Design brainstormed and locked (6 decisions, E1–E6); full 8-task TDD plan written. Not yet
executed — picks up with Task 1 next session.
- [x] Design spec written — `docs/superpowers/specs/2026-07-25-injury-muscle-link-design.md`
- [x] Implementation plan written — `docs/superpowers/plans/2026-07-25-injury-muscle-link.md`
- [ ] Task 1: migration (`0003_injury_muscle_link.sql`) + `domain::Muscle` + grown `Injury` + `InjuryPolicy` cross-field rule
- [ ] Task 2: persist `injury_type`/`muscle_id` through `PgInjuryRepository` + retrofit existing repo tests
- [ ] Task 3: `IMuscleRepository` port + `InMemoryMuscleRepository` fake
- [ ] Task 4: `PgMuscleRepository` + `AutoCommitPgMuscleRepository`
- [ ] Task 5: `InjuryService` muscle validation + update cascade + retrofit `test_injury_service.cpp`
- [ ] Task 6: `MuscleDto` + `MuscleController` (`GET /muscles`, `GET /muscles/{id}`)
- [ ] Task 7: extend `InjuryDto`/`InjuryController` for the new fields + retrofit `test_injury_controller.cpp`
- [ ] Task 8: wire `main.cpp` + live-verify

### Stage C — Frontend (`#25`) ⬜ NOT STARTED
- [ ] deps (react-hook-form, zod, @hookform/resolvers) + `api.ts` calls + MSW
- [ ] `ProfileForm` (RHF+Zod) + tests
- [ ] `InjuriesPanel` + tests (can now include a muscle picker via `GET /muscles`)
- [ ] `App` navigation to profile screen
- [ ] browser-automation verify + cleanup

---

## Decisions (see the design spec for full rationale)

The 8 slice-level decisions (D1–D8) live in the design spec. Highlights realized in Stage A:

### 2026-07-17 — Expand `domain::User`, not a separate `UserProfile` (D1)
Profile fields map 1:1 to the single `users` table; a separate entity would mean two types
reconciling against one table. Auth code ignores the new fields.

### 2026-07-17 — Load-modify-save for `PATCH`, no null-clearing in v1 (D2, D3)
`PATCH` parses into a `ProfileUpdate` of optionals (present = change), the service loads the user,
applies present fields, validates, persists via the existing `update()`. Absent = unchanged;
sending explicit `null` to *erase* a field is deferred.

### 2026-07-17 — Defer `preferences` (D4)
`preferences` (jsonb) is stored settings, **not** a recommendation engine. The "recommend
exercises the user will like" idea is a later subsystem that would *read* preferences among many
signals. Designing the column's shape before that consumer exists risks guessing wrong — leave it
untouched this slice.

### 2026-07-17 — Bounds philosophy: optional but sane (D7)
Biometrics are optional (unset ⇒ NULL, never forced). Provided values are **hard-rejected**
server-side if out of range (weight (0,500] kg, height (0,300] cm, enums, plausible date). The
client's Zod schema mirrors these as a friendly "please check" hint. Server enforces, client guides.

### 2026-07-25 — birth_date: calendar-sanity, not clock-coupled
`ProfilePolicy` validates `YYYY-MM-DD` shape + ranges (year 1900–2100, month, day) but deliberately
does **not** check "not in the future" against the wall clock — that would make the pure validator
(and its tests) time-dependent. The 2100 ceiling is enough for v1; a strict not-future check is a
documented deferral.

---

## Step-by-step build log

### Stage A — Profile (`#19`)

Executed on branch `slice-2-profile` via the executing-plans skill, following
`2026-07-17-slice-2a-profile-backend.md` task by task. Every task was a full TDD loop
(write test → watch red → implement → green → commit).

**A1 — domain + ProfilePolicy (`f88f45f`).** Grew `domain::User` with 9 profile fields (optionals;
`unitPreference` defaults to `"metric"`). Added `domain/ProfilePolicy.h`: `requireValidProfile()`
throwing `ProfileValidationError` that carries the offending field name (for RFC 9457 `errors`).
Red was the missing header; green = 12/12.

**A2 — persistence (`09531d5`).** Extended `PgUserRepository`'s `rowToUser()` + `SELECT`/`UPDATE`
SQL to map every profile column (`pqxx::params` handles `std::optional` ↔ NULL; `save()`
unchanged). Real red first: the round-trip test failed with `found->sex == nullopt` against live
Postgres before the fix. The in-memory fake needed **no change** (stores `User` by value) — added a
test to guard that. 21 repo tests green.

**A3 — UpdateProfile service (`cd86e58`).** `app::UpdateProfile::execute(userId, ProfileUpdate)`:
`findById` (missing → `ProfileUserNotFoundError`) → apply present fields → `requireValidProfile()`
guard **before** persisting → `update()`. 3 tests vs. the fake, incl. "invalid value persists
nothing." Green first try.

**A4 — HTTP layer (`e5b56d0`).** `http/dto/ProfileDto.h` (`parseProfileUpdate` reads only present
keys; `profileToJson` emits `null` for unset fields, never `password_hash`, excludes
`preferences`; `validationErrorResponse` builds the RFC 9457 `errors: {field:[...]}` object) + new
`http/controllers/UserController.cpp` (`registerUserRoutes`). GET + PATCH share **one Crow
multi-method route** (`.methods(Get, Patch)`, branch on `req.method`) — the plan's watch-item
(would Crow accept it?) resolved cleanly, no fallback needed. 6 HTTP tests via `HttpClient`; full
suite 94/94.

**A5 — wire + live-verify (`15eb144`).** Added `registerUserRoutes(app, users, tokens)` to
`main.cpp`, reusing the existing `AutoCommitPgUserRepository` + `JwtTokenService`. Built the server
clean, then **live-verified against the real server + Postgres** (not just tests): register → `GET`
(metric default, null biometrics) → `PATCH weight+experience` → `GET` (persisted) → invalid weight
`422` → no-token `401`. Every step matched; test user deleted from the dev DB afterward.

**One gotcha worth remembering:** a `cmake --build ... 2>&1 | tail -2 && run-tests` chain reported a
scary "ninja: build stopped" once — but the pipeline's exit code was `tail`'s, not cmake's, so a
real build failure would be masked (and here it was actually a transient that vanished on rebuild).
Confirmed the tree was clean with an explicit `cmake --build; echo exit=$?` → `exit=0`. When a
build's success gates the next step, check the real exit code, not piped tail output.

**Stage A result:** `ctest` **94/94** (up from 70 at Slice 1 close), live-verified. `#19` done.

### Stage B — Injuries (`#20`)

Executed on branch `slice-2-profile` following
`2026-07-25-slice-2b-injuries-backend.md`. A full new vertical stack mirroring the Slice-1 user
stack — so mostly "do what auth did, for a second entity."

**B1 — domain + policy (`c86a624`).** `domain::Injury` (id, userId, bodyRegion, severity, active,
notes, createdAt) + `InjuryPolicy` (body_region non-empty, severity ∈ {mild,moderate,severe}),
`InjuryValidationError` carrying the field name. 5 tests. `body_region` is free text in v1 —
tightening to a controlled vocabulary waits for the Slice-3 exercise catalog.

**B2 — port + fake (`013cb30`).** `IInjuryRepository` (create/listByUser/findById/update/
deleteById) + `InMemoryInjuryRepository` (vector-backed). 5 contract tests; `listByUser` scopes to
owner.

**B3 — Pg adapters (`f864966`).** `PgInjuryRepository` (takes `pqxx::work&`, real SQL incl.
`INSERT ... RETURNING`, `ORDER BY id`) + `AutoCommitPgInjuryRepository` (connection-per-call, same
pattern as the user repo). DB tests seed a user first (FK). 10 injury-repo tests green vs. real
Postgres.

**B4 — InjuryService (`af57de7`).** One cohesive service (not four use-case classes, per D5)
because add/list/update/remove share the ownership check: a private `requireOwned()` loads by id
and throws `InjuryNotFoundError` if missing **or** not owned by the caller — so another user's
injury is indistinguishable from a nonexistent one. 6 tests incl. both cross-user cases.

**B5 — HTTP layer (`e651976`).** `InjuryDto` + `InjuryController`: a **collection** route
(`GET`/`POST /users/me/injuries`) and an **item** route (`PATCH`/`DELETE
/users/me/injuries/<int>`), `InjuryNotFoundError` → **404**. 7 HTTP tests via `HttpClient`,
including cross-user PATCH and DELETE both returning 404. Full suite 122/122.

*Crow API gotcha (caught before coding, per the plan's watch-items):* verified the vendored
`crow_all.h` first — `crow::json::wvalue::list()` does **not** exist in this version. Built the
injuries JSON array via the `wvalue(std::vector<wvalue>)` constructor instead, which correctly
serializes an **empty** list as `[]` (not `null`). Also confirmed `.b()` (bool accessor) and
`<int>`→`int64_t` route-param binding exist before relying on them.

**B6 — wire + live-verify (`85dfcb5`).** Added `AutoCommitPgInjuryRepository` +
`registerInjuryRoutes` to `main.cpp`. **Live-verified vs. real server + Postgres:** create `201`
(created_at set by DB) → list → bad severity `422` → PATCH active=false `200` → DELETE `204` →
empty list `[]` → no-token `401`. Test user + injuries cleaned up (FK cascade).

**Stage B result:** `ctest` **122/122** (up from 94 after Stage A), live-verified. `#20` done.

---

## What's next

**Injury/muscle link (Stage B extension)** is designed and planned (spec + 8-task plan, both
committed) but **not yet executed** — pick up with Task 1 (migration + domain) next session,
executing inline (owner's preference, for visibility into each red→green→commit step). Then
**Stage C (`#25` frontend)**: profile + injuries forms (first React Hook Form + Zod in this
codebase), whose injury form can now build a muscle picker against `GET /muscles` once the
extension lands. Merge `slice-2-profile` → `main` when the whole slice is complete and verified.
