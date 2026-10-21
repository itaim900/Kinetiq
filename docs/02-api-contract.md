# User-Client API Contract — KinetiQ (v1)

**Status:** Approved (design phase)
**Last updated:** 2026-06-23
**Related:** [`01-data-model.md`](./01-data-model.md)

This is the public HTTP/REST contract between the React frontend and the C++ backend.
It is written contract-first: every endpoint lists the **test cases** it must satisfy, so
the test suite can be written before the implementation (TDD).

---

## Architectural Context

The backend follows **Ports & Adapters (Hexagonal Architecture)**. Core logic depends on
interfaces ("ports"); concrete implementations ("adapters") for Postgres, the LLM provider,
and HTTP are plugged in at the edges. In tests, real adapters are replaced with fakes/mocks.
This is what makes the codebase test-driven.

### Test layers

| Layer | Backend (C++ / GoogleTest) | Frontend (React / Vitest) |
|---|---|---|
| Unit | services, validators, calorie engine, JWT, safety filter — against mocked ports | components, hooks (+ React Testing Library) |
| Integration | repositories against an ephemeral Postgres (Docker) | API client against a mock server (MSW) driven by this contract |
| Contract / E2E | boot the app, hit real endpoints, assert against this contract | key full flows |

---

## Conventions

- **Base path:** `/api/v1` (URL versioning).
- **Auth:** JWT in `Authorization: Bearer <token>`. Endpoints marked 🔒 require it.
  - **v1 uses short-lived access tokens only (no refresh tokens).** On expiry the user
    re-authenticates. Refresh tokens are a planned later-phase enhancement.
- **Content type:** `application/json`.
- **Units:** always metric on the wire (`*_kg`, `*_cm`). The frontend converts for display
  based on `unit_preference`.
- **Error shape** (RFC 9457 Problem Details):

```json
{
  "type": "validation_error",
  "title": "Validation failed",
  "status": 422,
  "detail": "One or more fields are invalid.",
  "errors": { "email": ["must be a valid email address"] }
}
```

- **Status codes:** `200` ok · `201` created · `204` no content · `400` malformed ·
  `401` unauthenticated · `403` forbidden · `404` not found · `409` conflict ·
  `422` validation · `500` server · `502` upstream (LLM) failure.

---

## Auth

### `POST /auth/register`
Request: `{ email, password, display_name }`
Response `201`: `{ user, access_token }`

Password policy: **minimum 8 characters, at least one uppercase letter** (updated 2026-07-12,
supersedes the earlier length-only/10-char decision).

Test cases:
- ✅ valid input → `201`, returns token; `password`/`password_hash` never appear in response
- ❌ duplicate email → `409` (case-insensitive: `email` is `citext` — `Foo@x.com` collides with
  `foo@x.com`)
- ❌ invalid email, password < 8 chars, or password with no uppercase letter → `422` with field errors
- ❌ missing required field → `422`

### `POST /auth/login`
Request: `{ email, password }`
Response `200`: `{ user, access_token }`

Test cases:
- ✅ correct credentials → `200` + token
- ❌ wrong password → `401`, generic message (never reveal if the email exists)
- ❌ unknown email → `401`, identical message

**Known v1 limitation (2026-07-16, deliberate, not a bug):** the identical error *message* doesn't
fully close the no-enumeration goal — `LoginUser` short-circuits before hashing when the email
isn't found, so an unknown email responds measurably faster (~0ms) than a wrong password (~200ms+,
argon2's cost). A patient attacker could use response timing, not the message, to tell the two
apart. Accepted for v1 given this app's threat model (not a high-value target); the standard fix
is a dummy `verify()` call on the unknown-email path so both take similar time — revisit if that
threat model ever changes.

### `GET /auth/me` 🔒
Response `200`: current user profile.

Test cases:
- ✅ valid token → `200`
- ❌ missing / expired / invalid token → `401`

### `PATCH /auth/password` 🔒
Change password while logged in (distinct from the deferred "forgot password" reset flow below —
this one needs no email, the user already knows their current password).

Request: `{ current_password, new_password }`
Response `200`

Test cases:
- ✅ correct `current_password` + `new_password` passes policy → `200`; user can subsequently log
  in with the new password (and the old one no longer works)
- ❌ wrong `current_password` → `401`, generic message (same "don't leak more than necessary"
  principle as login)
- ❌ `new_password` fails the policy (< 8 chars or no uppercase) → `422`
- ❌ unauthenticated (missing/expired/invalid token) → `401`

**Logout:** no endpoint. v1 uses short-lived, stateless JWTs with no server-side session (see
Conventions above) — "logging out" is just the client discarding its stored token.

---

## Profile & Biometrics

### `GET /users/me` 🔒
Returns full profile: biometrics, preferences, `experience_level`, goals.

### `PATCH /users/me` 🔒
Partial update. Request (any subset):
`{ height_cm, weight_kg, birth_date, sex, experience_level, unit_preference,
   weekly_workout_goal, available_time_minutes, preferences, medications }`

Test cases:
- ✅ partial update changes only provided fields → `200`
- ❌ `weight_kg` negative or out of range → `422`
- ❌ `experience_level` not in `{beginner,intermediate,advanced}` → `422`
- ❌ unauthenticated → `401`

---

## Injuries (drives the rule-based safety filter)

An injury is either a **known condition** from the catalog (region, type and muscles come from
the catalog and are **locked**) or an **unknown injury** the user describes. Design:
`docs/superpowers/specs/2026-10-21-injury-conditions-catalog-design.md`.

### `GET /users/me/injuries` 🔒 — list
### `POST /users/me/injuries` 🔒 → `201`
Two request shapes:
```jsonc
// Known condition
{ "condition_id": 1, "severity": "moderate", "notes": "optional" }
// Unknown injury (muscle_ids optional; duplicates removed; role "primary")
{ "body_region": "wrist_hand", "injury_type": "joint", "muscle_ids": [17, 18],
  "severity": "mild", "notes": "optional" }
```
- `injury_type`: `muscle | bone | tendon_ligament | joint | other`; `body_region`: a slug from
  `GET /body-regions`.
- Known condition + any of `body_region` / `injury_type` / `muscle_ids` → `422` naming that field.
### `PATCH /users/me/injuries/{id}` 🔒 → `200`
Partial update. Known condition: only `severity`, `notes`, `active`; locked fields → `422`. To change
the condition, delete and re-add. Unknown injury: also `body_region`, `injury_type`, `muscle_ids`
(**replaces** the set; `[]` clears it).
### `DELETE /users/me/injuries/{id}` 🔒 → `204`

Response (every injury):
```jsonc
{ "id": 7, "user_id": 3,
  "condition": { "id": 1, "name": "ACL tear", "icd11_code": "NC93.62" },   // null if unknown
  "body_region": "knee", "injury_type": "tendon_ligament", "severity": "moderate",
  "muscles": [ { "id": 14, "name": "Quads", "role": "secondary" } ],     // primary first
  "active": true, "notes": null, "created_at": "..." }
```

Test cases:
- ✅ create injury (either shape) → appears in list
- ✅ known condition → region/type/muscles from the catalog
- ❌ `severity` not in `{mild,moderate,severe}` / bad `injury_type` → `422`
- ❌ unknown `condition_id`, `body_region` or muscle id → `422` naming the field
- ❌ known condition + locked field (POST or PATCH) → `422` naming the field
- ❌ PATCH/DELETE an injury owned by another user → `404` (do not leak existence)

---

## Reference catalog (read-only) 🔒

### `GET /body-regions` — `[{slug, name}]` (14 fixed regions)
### `GET /muscles` — `[{id, name, muscle_group}]` (28)
### `GET /injury-conditions` — `[{id, slug, name, icd11_code|null, body_region, injury_type, muscles: [{id, name, role}]}]` (17)

Conditions are general information, **not medical advice**; the UI must say so. ICD-11 codes and
muscle sources are listed in the conditions-catalog spec. No token → `401`.

---

## Exercise Catalog (read-only for clients)

### `GET /exercises` 🔒 — filterable, paginated
Query: `?muscle=chest&equipment=barbell&type=strength&difficulty=3&page=1&page_size=20`
Response: `{ items: [...], page, page_size, total }`

Test cases:
- ✅ no filters → paginated full list
- ✅ `muscle=chest` → only exercises targeting chest
- ✅ combined filters → correct intersection
- ✅ `page_size` capped at a maximum (e.g. 100)

### `GET /exercises/{id}` 🔒
Full detail incl. muscles, equipment, instructions, media.

Test cases:
- ✅ existing id → `200`
- ❌ unknown id → `404`

---

## Workouts

### `GET /workouts` 🔒
User's workouts, filterable by `status`.

### `GET /workouts/{id}` 🔒
Workout + ordered exercises with prescriptions.

Test cases:
- ❌ another user's workout → `404`

### `POST /workouts` 🔒 — manual create
Request:
```json
{
  "name": "Push Day",
  "workout_type": "strength",
  "target_environment": "commercial_gym",
  "exercises": [
    { "exercise_id": 12, "sets": 4, "target_reps_min": 8, "target_reps_max": 12,
      "target_weight_kg": 60, "rest_seconds": 120, "order_index": 0 }
  ]
}
```
Response `201` (with `source = "manual"`).

Test cases:
- ✅ valid → `201`, `source = "manual"`
- ❌ references a non-existent `exercise_id` → `422`

### `PATCH /workouts/{id}` 🔒
Rename, change `status` (`active`/`archived`), reorder/edit exercises.

### `DELETE /workouts/{id}` 🔒 → `204`

### `POST /workouts/generate` 🔒 — the Phase 3 AI endpoint
**Synchronous in v1**: the client waits for the LLM (~2–10s) and receives the workout in the
same response.

Request: `{ workout_type, target_environment, focus_muscles?: [], exclude_muscles?: [], notes? }`

Flow:
1. Load user profile.
2. **Rule-based safety filter** removes contraindicated exercises (JOIN on `User_Injuries` ⨝ `Exercise_Contraindications`).
3. Build the LLM payload from the filtered pool + constraints.
4. Call the LLM via the `ILLMClient` port. The LLM returns structure only (exercises, sets, reps, rest) — **never weights**.
5. **Validate** the response; on failure, retry with feedback (self-correction) up to N times.
6. **Deterministic Starting-Weight Engine** fills `target_weight_kg` + `weight_source` per exercise: if the user has sex + bodyweight and the exercise has a strength norm, suggest `multiplier × bodyweight` (`weight_source = "body_type_estimate"`); otherwise leave `null` (`weight_source = "needs_user_input"`).
7. Persist the workout + the `Generation_Requests` audit row.
8. Return the workout (including `generation_request_id` and per-exercise `weight_source`).

Each workout exercise in responses carries `target_weight_kg` and `weight_source`
(`body_type_estimate` | `needs_user_input` | `user_set`) so the client can show the right
message. The user can always override the weight, which sets `weight_source = "user_set"`.

Test cases (run against a **mocked `ILLMClient`** — fast, free, deterministic):
- ✅ valid request + valid mocked response → `201` workout linked to a `Generation_Requests` row
- ✅ a contraindicated exercise (user has `lower_back` injury) is **never** in the result, even if the mock LLM returns one → rejected by validation
- ✅ mock LLM returns malformed JSON → backend retries; after N failures → `502`, failure logged in `Generation_Requests`
- ✅ generated total duration respects `available_time_minutes`
- ✅ user with sex + bodyweight + a normed exercise → `target_weight_kg` set, `weight_source = "body_type_estimate"`
- ✅ user missing sex/bodyweight, or non-normed exercise → `target_weight_kg = null`, `weight_source = "needs_user_input"`
- ❌ unauthenticated → `401`

---

## Deferred to later phases

- **Google OAuth login** — sign in with a Google account. Deferred out of Slice 1 (2026-07-12
  decision): it needs a Google Cloud OAuth client, server-side ID token verification against
  Google's JWKS, and a schema change (nullable `password_hash`, an auth-provider marker on
  `Users`) — bigger than a test case, treat as its own future slice once email/password auth
  (register/login/me) is done.
- **"Forgot password" reset flow** — deferred (2026-07-12 decision), same shape of reason as
  OAuth: needs an email-sending dependency (nothing like that exists in the stack), a time-limited
  single-use reset-token table, and its own security design (token expiry, rate-limiting the
  request endpoint, and re-litigating the no-enumeration question for "is this email registered"
  in the reset-request step too). **Change password while logged in** (above) ships in Slice 1 and
  covers the common case that doesn't need email.
- **Account deletion** — deferred (2026-07-12 decision). Not just a missing `DELETE /users/me`:
  needs a design for re-authentication before a destructive action, and a decision on soft- vs
  hard-delete given the FK fan-out (workouts, injuries, generation requests all reference `Users`).
- **Rate limiting / lockout after repeated failed logins** — real security value, needs a counter
  store and its own design; not in v1.
- **Email verification on registration** — bundle with the reset flow above when email infra is
  built.
- **Refresh tokens** (silent re-auth) — enhancement to the auth scheme above.
- **Asynchronous `/generate`** (`202 Accepted` + job polling / SSE) — only if latency becomes a problem.
- **Workout history & feedback endpoints** (Phase 4): logging completed sessions, RPE, set-level logs.
- **Friend workout updates** (Phase 4): motivational feed, no biometric/medical data exposed.

---

## Next artifact

Formalize this contract as an **OpenAPI** file (`docs/openapi.yaml`) once the `/generate`
shape is finalized by the LLM port design. OpenAPI enables auto-generated docs, generated
TypeScript client types, mock servers, and contract tests.
