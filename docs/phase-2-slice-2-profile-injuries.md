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

### Stage B extension — Injury conditions catalog (2026-10-21)

**Part 1 — `injury_type` (July plan, Tasks 1–2, commit `2717e92`).** Migration `0003` added
`injury_type` (5-value CHECK) and a single optional `muscle_id`, seeded 16 muscles. Task 1 alone
left 10 tests red (empty `injury_type` now invalid), which the new pre-commit hook would block, so
Tasks 1+2 plus the `injury_type` plumbing through service + DTO were done together. Every commit green.

**Part 2 — redesign: several muscles per injury.** Owner wanted injuries to reflect their real
medical effect. Re-brainstormed (spec `2026-10-21-injury-conditions-catalog-design.md`, superseding
July's E2/E4/E5): a catalog of 17 known conditions, each anchored to a WHO ICD-11 code (14/17; 3
not found, left NULL) and mapped to several muscles with a role (primary = injured tissue,
secondary = involved / to protect). **Known condition = locked** (region, type, muscles come from
the catalog); **unknown injury** = user picks region (fixed list of 14), type and muscles. Research
found no open database mapping injuries → muscles; SNOMED CT "finding site" only gives the primary
structure, so secondary muscles are project judgement pending physio review.

Built in 5 TDD tasks (plan `2026-10-21-injury-conditions-catalog.md`):
1. `ICatalogRepository` port + domain types + in-memory fake (`b712262`).
2. Migration `0004` (body regions + FKs, 28 muscles, 17 conditions, 34 muscle links) +
   `PgCatalogRepository` / `AutoCommitPgCatalogRepository`; tests pin the exact counts because a
   mistyped muscle name would silently drop a link (`1940210`).
3. Migration `0005` (drop `muscle_id`; add `condition_id`, `user_injury_muscles`) + `Injury`
   gains `condition` + `muscles`; `PgInjuryRepository` reads muscles from the catalog or the user's
   picks (`a4cc43e`).
4. `InjuryService(injuries, catalog)`: the two shapes, locked fields → 422 naming the field,
   region/muscle existence checks, de-duplication (`44b5849`).
5. `CatalogController` (`GET /body-regions`, `/muscles`, `/injury-conditions`) + new injury JSON
   (`066db8c`).

*Plan deviation:* two migrations (0004 catalog, 0005 injury link) instead of one, so the catalog
commit stayed green before the injury tables changed.

**Verified:** `kinetiq_tests` **171/171, 0 skipped**; migrations checked with `psql`; live `curl`:
ACL tear → 201 with `NC93.62` + 3 secondary muscles; ACL + `body_region` → 422 on that field;
unknown wrist injury → 201 with `condition: null`, `muscles: []`; `left_knee` → 422; no token → 401.

---

### Stage C — Frontend (`#25`) (2026-10-22 → 2026-10-25)

Designed with browser mockups (spec `2026-10-22-stage-c-frontend-design.md`, S1–S8): top bar with
Injuries/Profile tabs, one searchable condition field ("My injury isn't listed…" → region/type/
muscles), inline add/edit forms, not-medical-advice notice. React Router 7 + React Hook Form + Zod
+ plain `fetch` (no TanStack Query).

Built in 8 TDD tasks: rename **KinetiQ → Kinetiq** · `authFetch`/`ApiError`/401 logout + typed API +
stateful MSW · `AppShell` + routing · injuries list (cards, healed section, delete confirm) · pure
`injurySchema.ts` (Zod rules + add/update request converters) · `InjuryForm` (`ConditionSearch`,
locked catalog muscles, `MuscleSelect`) · add/edit on the page · profile form.

**Additions** (spec `2026-10-25-themes-units-clearing-design.md`, 12 tasks): repo renamed
`itaim900/Kinetiq`; Compose fixed (fixed project/volume names, data copied + verified); `npm audit`
→ 0 production advisories (**zod 3 → 4.6.7**); ADRs 0013–0017; light/night themes with a
monochrome accent (CSS tokens + `data-theme`, no-flash inline script); app-wide **Metric |
Imperial** switch saved to the account (server stays metric); `PATCH /users/me` = **JSON Merge
Patch** (`null` clears) + **`user_profile_history`** (migration 0006). Lessons: a flaky App test
under parallel load (async timeout raised to 3 s); the compiler ran out of memory on a wide rebuild
(hooks/CLAUDE.md now use `-j 2`).

**Browser verification (Playwright, real server + Postgres, 2026-10-25):** login screen light →
night; register → `/injuries`; ACL tear (ICD-11 NC93.62, 3 locked secondary muscles) added; unlisted
wrist injury with Forearm Flexors added; ACL edited (severity + notes); wrist marked healed →
"Healed (1)" → deleted after confirmation; profile saved (180 cm / 75 kg / intermediate); Imperial →
70.9 in "= 5 ft 11 in" / 165.3 lb; reload keeps Imperial (account) and night (browser); back to
Metric → exactly 180/75; weight cleared → "Kinetiq will keep using your last saved weight (75 kg)…",
DB: `weight_kg` NULL + history rows incl. `75 → (null)`; light mode in-app (primary pill filled,
secondary outlined); back button `/injuries` → `/profile`. Only console error: missing `favicon.ico`.
**Bug found + fixed:** after saving, switching units rebuilt the profile form from the profile
loaded at page open, so just-saved values vanished — `ProfilePage` now updates its copy after each
save (regression test added). Test user deleted (history cascaded).

**Result:** client 94/94, server 184/184 (0 skipped). Slice 2 complete; merge to `main` pending the
owner's go-ahead.

---

## What's next

**Merge `slice-2-profile` → `main`** (owner's go-ahead), then **Slice 3 — Exercise catalog + seed**
(`#21`, `#22`, `#26`). Deferred items live in `docs/backlog.md`.
