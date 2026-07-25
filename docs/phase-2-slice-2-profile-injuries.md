# Phase 2, Slice 2 — Profile & Injuries: Step-by-Step Build Log

**Status:** 🚧 IN PROGRESS (started 2026-07-25). Stage A (`#19`) ✅ done. Stage B (`#20`) + Stage C (`#25`) pending.
**Issues:** #19 (`GET`/`PATCH /users/me`), #20 (injuries CRUD), #25 (React profile + injuries forms)
**Design spec:** [`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`](./superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md) — the *what & why* (8 locked decisions)
**Stage A plan:** [`docs/superpowers/plans/2026-07-17-slice-2a-profile-backend.md`](./superpowers/plans/2026-07-17-slice-2a-profile-backend.md) — the bite-sized TDD steps
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

### Stage B — Injuries (`#20`) ⬜ NOT STARTED
- [ ] Stage B TDD plan written
- [ ] `domain::Injury` + validation
- [ ] `ports::IInjuryRepository` + `InMemoryInjuryRepository` fake
- [ ] `db::PgInjuryRepository` + `AutoCommitPgInjuryRepository`
- [ ] `app::InjuryService` (add/list/update/remove + ownership 404)
- [ ] `http/dto/InjuryDto.h` + `InjuryController` (GET/POST/PATCH/DELETE)
- [ ] wired into `main.cpp`, live-verified

### Stage C — Frontend (`#25`) ⬜ NOT STARTED
- [ ] deps (react-hook-form, zod, @hookform/resolvers) + `api.ts` calls + MSW
- [ ] `ProfileForm` (RHF+Zod) + tests
- [ ] `InjuriesPanel` + tests
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

---

## What's next

Stage B (`#20` injuries) — write its own bite-sized plan
(`docs/superpowers/plans/2026-07-25-slice-2b-injuries-backend.md`) mirroring Stage A's, then execute:
`Injury` domain + validation → `IInjuryRepository` + fake → `Pg`/`AutoCommit` adapters →
`InjuryService` (with the ownership-check 404) → `InjuryController` → wire + verify. Then Stage C
(`#25` frontend, first React Hook Form + Zod). Merge `slice-2-profile` → `main` when the slice is
complete and verified.
