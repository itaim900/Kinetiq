# Phase 2, Slice 2 — Profile & Injuries: Design Spec

**Status:** Approved design (brainstormed 2026-07-17) — ready for implementation planning
**Issues:** `#19` (`GET`/`PATCH /users/me`), `#20` (injuries CRUD), `#25` (React profile + injuries forms)
**Contract:** [`docs/02-api-contract.md`](../../02-api-contract.md) — Profile & Injuries sections (source of truth for request/response shapes + test cases)
**Data model:** [`docs/01-data-model.md`](../../01-data-model.md) — `Users`, `User_Injuries`
**Precedent:** [`docs/phase-2-slice-1-auth.md`](../../phase-2-slice-1-auth.md) — the patterns this slice reuses

---

## Goal

Let an authenticated user (a) view and partially update their full profile — biometrics,
preferences metadata, experience level, goals — and (b) manage their injuries (create, list,
edit, delete). Injuries are the data layer the Phase 3 rule-based safety filter will JOIN against,
so this slice is a prerequisite for AI generation.

Every endpoint is authenticated (🔒) and builds on Slice 1's `authenticate()` function.

---

## Key findings (what shapes the work)

1. **No migration needed.** `0002_core_schema.sql` (built up-front in Slice 0) already contains
   every profile column on `users` (`sex`, `birth_date`, `height_cm`, `weight_kg`,
   `experience_level`, `unit_preference`, `weekly_workout_goal`, `available_time_minutes`,
   `preferences` jsonb, `medications`) and the full `user_injuries` table (FK to `users` with
   `ON DELETE CASCADE`, `severity` CHECK, `active` default true, `idx_user_injuries_user_id`).
   Slice 2 is pure application/adapter/UI work on an existing schema.

2. **`domain::User` is deliberately narrow today** — `id, email, passwordHash, displayName` only
   (its own comment says "biometrics etc. arrive in Slice 2"). Growing it is the pivot of `#19`.

3. **Profile reuses existing machinery; injuries are a new parallel stack.** `IUserRepository`
   already has `findById` + `update`, and `AutoCommitPgUserRepository` (connection-per-call) is
   ready to reuse. So `#19` grows what exists. `#20` copies the *entire* Slice-1 user stack shape
   for a second entity — good reinforcement of the pattern.

---

## Locked decisions (this slice)

### D1 — Domain model: expand `domain::User` (not a separate `UserProfile`)
Add the profile fields directly onto `User`. Rationale: the data maps 1:1 to the single `users`
table; a separate entity would mean two types reconciling against one table. Auth code simply
ignores the new fields.

### D2 — Partial update: load-modify-save in the service (not dynamic SQL)
`PATCH /users/me` parses into a `ProfileUpdate` struct of optionals (present = change this field).
The service `findById`s the current user, applies only the present fields, then calls the existing
`update()` with the full `User`. Type-safe, unit-testable against the in-memory fake, reuses
`update()`. Costs one extra `SELECT` per PATCH — negligible.

### D3 — Update semantics: absent = unchanged, present = set. No null-clearing in v1.
Sending a field sets it; omitting it leaves it alone. Explicitly **not** supporting "send `null` to
erase a field" (e.g. clear your saved weight) — it complicates the optional model for a rare need.
Deferred; addable later.

### D4 — `preferences` (jsonb): deferred from Slice 2 entirely
`preferences` is *stored user settings* (a flat bag: preferred environments, splits, disliked
equipment). It is **not** a recommendation engine. The percentage-based "recommend exercises the
user will like" idea is a separate, later subsystem (Phase 3 safety/variety + Phase 5 scoring) that
would *read from* `preferences` among many signals. Designing the `preferences` shape before that
consumer exists risks guessing wrong, so we leave the column untouched this slice and design it when
its consumer is built. Biometrics + goals + injuries make the profile fully useful without it.

### D5 — Injuries: one cohesive `app::InjuryService` (not four use-case classes)
A small, deliberate deviation from Slice 1's one-class-per-use-case grain (`RegisterUser` etc.),
justified because the four CRUD operations are tiny and **share the ownership check** — keeping it
as one private method both `update` and `remove` call is cleaner than duplicating it across four
files or a free helper.

### D6 — Keep `/auth/me` and `/users/me` distinct (separate serializers)
`/auth/me` stays the minimal identity check (`id, email, display_name`) the client hits on page
load to restore a session; `/users/me` returns the full profile for the profile screen. Two
serializers, but the frequent lightweight session-check call never bloats as the profile grows, and
`auth` vs `users` stay semantically clean resources. (Rejected the merge-into-one-serializer option
— it optimized for fewer lines at the cost of a muddier API.)

### D7 — Validation lives in the domain (mirrors `PasswordPolicy`)
Field rules (ranges, enums, valid date) live in a pure, DB-free `domain::ProfilePolicy` +
injury validation — unit-testable without a database, same as `PasswordPolicy`. The client's Zod
schema mirrors these rules (validated both sides).

**Bounds philosophy (owner decision 2026-07-17):** biometric fields are the user's own to fill in
or leave blank — all optional, an unfilled field is simply NULL ("n/a"), never forced. But input
that *is* provided must be sane: the **server hard-rejects** negatives, zero, and physically
implausible values (weight `> 0` and `≤ 500` kg, height `> 0` and `≤ 300` cm) with a `422` — this
is the anti-abuse guardrail; the server never trusts the client. The matching **client Zod schema
surfaces the same limits as a friendly inline "please check this value" suggestion** *before*
submit. Server enforces, client guides — same rule, two layers.

### D8 — Error shape: RFC 9457 with per-field `errors`
Validation failures return `422` with the contract's `errors: { field: ["message"] }` object — a
small step up from Slice 1's generic `detail`-only messages, honoring the shape `02-api-contract.md`
already specifies. Achieved by validation errors carrying the offending field name.

---

## `#19` — Profile (`GET`/`PATCH /users/me`)

### Domain
- **Grow `domain/User.h`** with (nullable → `std::optional`): `sex`, `birthDate` (ISO `YYYY-MM-DD`
  string), `heightCm` (double), `weightKg` (double), `experienceLevel`, `weeklyWorkoutGoal` (int),
  `availableTimeMinutes` (int), `medications` (string). `unitPreference` is non-optional
  `std::string` defaulting to `"metric"` (matches the DB NOT NULL DEFAULT). `preferences` is **not**
  added to the domain type this slice (D4).
- **New `domain/ProfilePolicy.h`** — `validateProfile(...)` (or per-field helpers) throwing
  `ProfileValidationError(field, message)`:
  - `weight_kg`: > 0 and ≤ a sane max (e.g. 500)
  - `height_cm`: > 0 and ≤ a sane max (e.g. 300)
  - `experience_level` ∈ {beginner, intermediate, advanced}
  - `sex` ∈ {male, female, other, unspecified}
  - `unit_preference` ∈ {metric, imperial}
  - `weekly_workout_goal` ≥ 0
  - `available_time_minutes` > 0
  - `birth_date`: parseable date, not in the future

### Ports — no new methods
`findById` + `update` already exist. Work is: extend `PgUserRepository`'s `SELECT`/`UPDATE` SQL and
`rowToUser()` to map the profile columns; `update()` now persists them too. `save()` (register)
untouched — profile stays null/default at signup. The `InMemoryUserRepository` fake already stores a
whole `User`, so once `User` grows it round-trips the new fields automatically (verify with a test).

### App
- **`app::UpdateProfile`** — `execute(userId, ProfileUpdate)`: `findById` (defensive: missing →
  throw, like `ChangePassword`) → apply present fields onto the loaded `User` → `ProfilePolicy`
  validation (guard-clause style) → `update()` → return the updated `User`.
- **GET needs no service** — the controller calls `users.findById(currentUser.userId)` directly
  (same as `/auth/me` today).

### HTTP
- **New `http/dto/ProfileDto.h`** — `parseProfileUpdate(json) -> ProfileUpdate` (reads only present
  keys), `profileToJson(User)` (full profile; never emits `passwordHash`).
- **New `http/controllers/UserController.cpp`** — `registerUserRoutes(app, users, tokens)`:
  - **`GET /api/v1/users/me`** 🔒 → 200 full profile (401 if unauth).
  - **`PATCH /api/v1/users/me`** 🔒 → 200 updated profile; 422 on validation (field errors); 401 if
    unauth; 400 on malformed JSON.
- Wire `registerUserRoutes` into `main.cpp`, reusing the existing `AutoCommitPgUserRepository`.

### Test cases (from contract)
- ✅ partial update changes only provided fields → 200
- ❌ `weight_kg` negative/out of range → 422
- ❌ `experience_level` not in enum → 422
- ❌ unauthenticated → 401

---

## `#20` — Injuries (`/users/me/injuries` CRUD)

A full new stack mirroring the Slice-1 user stack.

### Domain
- **`domain/Injury.h`** — `id, userId, bodyRegion, severity, active (default true), notes,
  createdAt`.
- **Validation** — `severity` ∈ {mild, moderate, severe}; `body_region` non-empty.
  - *Deferral:* the contract notes `body_region` should eventually match
    `Exercise_Contraindications` values, but that catalog isn't seeded until Slice 3. v1 accepts
    **free-text non-empty** region; tightening to a controlled vocabulary is a documented Slice-3
    follow-up.

### Port + fake
- **`ports/IInjuryRepository.h`** — `create(Injury) -> Injury` (assigns id), `listByUser(userId) ->
  vector<Injury>`, `findById(id) -> optional<Injury>`, `update(Injury)`, `deleteById(id)`.
- **`tests/support/InMemoryInjuryRepository.h`** — fake.

### Adapters (Slice-1 split)
- **`adapters/db/PgInjuryRepository.{h,cpp}`** — takes a `pqxx::work&` it doesn't own (composes with
  `DbTest` rollback), fixed SQL.
- **`adapters/db/AutoCommitPgInjuryRepository.{h,cpp}`** — production wrapper, connection+transaction
  per call. Same rationale as `AutoCommitPgUserRepository`.

### App
- **`app::InjuryService`** — `add(userId, ...) -> Injury`, `list(userId) -> vector<Injury>`,
  `update(userId, injuryId, ...)`, `remove(userId, injuryId)`.
- **Ownership rule (D5, straight from contract):** `update`/`remove` `findById` first; if missing
  **or** `injury.userId != userId` → throw `InjuryNotFoundError` → controller returns **404** (never
  reveal another user's injury exists). One private helper both methods call.

### HTTP
- **`http/dto/InjuryDto.h`** — parse create/update requests, `injuryToJson`.
- **`http/controllers/InjuryController.cpp`** — `registerInjuryRoutes(app, injuries, tokens)`:
  - `GET /api/v1/users/me/injuries` 🔒 → 200 list
  - `POST /api/v1/users/me/injuries` 🔒 → 201 created (422 bad severity)
  - `PATCH /api/v1/users/me/injuries/{id}` 🔒 → 200 (404 if not owned/absent)
  - `DELETE /api/v1/users/me/injuries/{id}` 🔒 → 204 (404 if not owned/absent)
- Wire into `main.cpp` with `AutoCommitPgInjuryRepository`.

### Test cases (from contract)
- ✅ create injury → appears in list
- ❌ `severity` not in enum → 422
- ❌ PATCH/DELETE an injury owned by another user → 404 (no existence leak)
- ❌ unauthenticated → 401

---

## `#25` — Frontend (first React Hook Form + Zod)

### Setup + API
- Add `react-hook-form`, `zod`, `@hookform/resolvers`.
- `src/api.ts`: authed calls (Bearer token, reusing the existing RFC-9457 error-parsing helper):
  `getProfile`, `updateProfile`, `listInjuries`, `addInjury`, `updateInjury`, `deleteInjury`.

### Components
- **`features/profile/ProfileForm.tsx`** — RHF + a Zod schema **mirroring `ProfilePolicy`**
  (same rules both sides). Prefilled from `GET /users/me`; submits `PATCH`.
- **`features/profile/InjuriesPanel.tsx`** — list current injuries, add new, mark inactive / delete.
- **`App.tsx`** — the authenticated view (currently minimal) gains navigation to a profile screen.

### Tests
- Vitest + Testing Library + MSW handlers for all new endpoints (mirrors the Slice-1 client tests).

### Deferral
- Wire is always metric; **imperial display conversion deferred** — v1 forms are metric-only to keep
  the first RHF/Zod introduction focused. `unit_preference` is still stored/editable.

---

## Testing strategy (per stage, up the pyramid)

| Layer | Profile (`#19`) | Injuries (`#20`) |
|---|---|---|
| Domain unit | `test_profile_policy.cpp` | injury validation tests |
| Service unit (fakes) | `test_update_profile.cpp` | `test_injury_service.cpp` (incl. ownership 404) |
| Adapter integration (`DbTest`) | extend `test_pg_user_repository.cpp` (profile columns round-trip) | `test_pg_injury_repository.cpp` |
| HTTP in-process (`HttpClient`) | `test_user_controller.cpp` (200/422/401) | `test_injury_controller.cpp` (200/201/204/404/422/401) |
| Client (Vitest + MSW) | `ProfileForm` tests | `InjuriesPanel` tests |

Each **backend** stage ends with a **curl live-verify** against the real server + Postgres. The
**frontend** stage ends with **browser-automation verification** + test-user cleanup — same
discipline as Slice 1.

---

## Staging (workflow)

| Stage | Issue | Steps |
|---|---|---|
| **A** | #19 | A1 grow `User` + `ProfilePolicy` (TDD) → A2 extend `PgUserRepository` + fake mapping (TDD, `DbTest`) → A3 `UpdateProfile` service (TDD) → A4 `ProfileDto` + `UserController` GET/PATCH (HTTP TDD) → A5 wire `main.cpp` + curl-verify |
| **B** | #20 | B1 `Injury` + validation (TDD) → B2 `IInjuryRepository` + `InMemory` fake (TDD) → B3 `Pg` + `AutoCommit` adapters (TDD, `DbTest`) → B4 `InjuryService` + ownership (TDD) → B5 `InjuryDto` + `InjuryController` (HTTP TDD) → B6 wire `main.cpp` + curl-verify |
| **C** | #25 | C1 deps + `api.ts` calls + MSW → C2 `ProfileForm` (RHF+Zod) + tests → C3 `InjuriesPanel` + tests → C4 `App` navigation → C5 browser-verify + cleanup |
| **D** | — | Docs: append `WORKLOG.md`, update `CONTEXT.md`, maintain a living `docs/phase-2-slice-2-profile-injuries.md` build log; optionally capture deferred ADRs (0013+) |

---

## Deferrals (documented, not accidental)

- **Null-clearing on PATCH** (send `null` to erase a field) — D3.
- **Structured `preferences` editing** — until its consumer (recommender) exists — D4.
- **`body_region` controlled vocabulary** — until the exercise catalog is seeded (Slice 3).
- **Imperial display conversion** on the client — `unit_preference` stored, conversion later.

---

## Open items / future ADRs

- The Slice-1 doc flagged capturing Phase 2 locked decisions + auth-as-authenticator as ADRs
  (0013+). This slice's D1 (expand-User) and D2 (load-modify-save) are candidates too — optional,
  during Stage D.
- Reuse note (carried from Slice 1): any repository wired directly into `main.cpp` uses the
  connection-per-call `AutoCommit*` pattern, never a single shared long-lived transaction.
