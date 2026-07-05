# Phase 2 Plan — Core Domain

**Status:** In progress (started 2026-07-05)
**Milestone:** Phase 2 — Core Domain (GitHub milestone #3, 13 issues #15–#27)
**Depends on:** Phase 1 complete (`docs/phase-1-summary.md`)
**Contract:** every endpoint + its test cases are defined in `02-api-contract.md`
**Data model:** all tables defined in `01-data-model.md`

---

## 1. Goal

All the **non-AI** functionality, built **test-first**. Definition of Done: a user can register, log
in, manage their profile + injuries, browse a seeded exercise catalog, and build/save a workout by
hand — all covered by passing unit/integration/contract tests. (AI generation is Phase 3.)

---

## 2. Approach: thin vertical slices, auth first

Instead of "all backend, then all frontend", we build **vertical slices**. Each slice cuts through
every layer (DB → repository port → domain/app logic → HTTP endpoint → React UI) and leaves something
demoable and tested. **Auth is first** because every other endpoint requires a logged-in user (🔒).

Per-layer, the backend follows **Ports & Adapters** (ADR-0002): core logic depends on interfaces
("ports"); Postgres/HTTP/LLM are "adapters" plugged in at the edges. Tests swap real adapters for
fakes.

---

## 3. Locked decisions

| # | Decision | Choice | Rationale |
|---|---|---|---|
| 1 | Migration runner | **C++ runner on backend startup** | One mechanism, identical in dev + containers; applies `migrations/NNNN_*.sql` not yet recorded in `schema_migrations`, in order |
| 2 | Integration-test DB | **Dedicated test DB + per-test transaction rollback** | Fast and fully isolated; no leftover rows between tests |
| 3 | Password hashing | **argon2** (`libargon2` / `libargon2-dev`) | Modern, memory-hard; prebuilt on both MSYS2 and Ubuntu |
| 3 | JWT | **jwt-cpp** (header-only, via CMake `FetchContent`) | Header-only avoids the ABI/packaging pain we hit with libpqxx |
| 4 | Schema granularity | **Full v1 schema up front** (issue #15) | The data model is already locked in `01-data-model.md`; de-risks later slices |

> The 4 decisions above are good candidates for ADRs (e.g. ADR-0013 migration runner, ADR-0014
> test-DB strategy, ADR-0015 auth libraries) when we formalize them.

---

## 4. Target backend structure (Ports & Adapters)

Introduced incrementally starting in Slice 0 (see `04-architecture.md` for the full vision):

```
Server/src/
├── main.cpp            # composition root: build adapters, inject, start server
├── config/             # env/config loading (Env.h lives here today)
├── http/               # inbound HTTP adapter
│   ├── controllers/    # AuthController, UserController, ...
│   ├── middleware/     # JWT auth -> CurrentUser, error mapping
│   └── dto/            # request/response DTOs + (de)serialization
├── app/                # application services (use cases)
├── domain/             # entities + validators (pure, no I/O)
├── ports/              # interfaces: IUserRepository, ITokenService, ...
└── adapters/
    └── db/             # Postgres repository implementations (libpqxx)
```

---

## 5. Slices

### Slice 0 — Foundations (prep; no user-facing feature)
The groundwork every later slice needs. Detailed breakdown in §6.
- Hexagonal backend folders.
- Wire **argon2** + **jwt-cpp** into CMake (verify on MSYS2 **and** CI/Docker).
- **Startup migration runner** (+ fold in existing `0001_init`).
- **Integration-test DB harness** (dedicated DB + transaction rollback).
- **Full v1 schema migration** from `01-data-model.md`.

### Slice 1 — Auth  (issues #16, #17, #18, #24)
- `IUserRepository` port + in-memory fake.
- argon2 password hashing; JWT issue/verify; auth middleware → `CurrentUser`.
- `POST /auth/register`, `POST /auth/login`, `GET /auth/me`.
- Frontend: register/login flow, in-memory token handling.
- **Contract tests:** duplicate email → 409; wrong password → 401 (generic); token required on `me`.

### Slice 2 — Profile & Injuries  (#19, #20, #25)
- `GET`/`PATCH /users/me` (partial update, validation).
- Injuries CRUD (`/users/me/injuries`).
- Frontend: profile + injuries forms (React Hook Form + Zod).
- **Contract tests:** out-of-range `weight_kg` → 422; another user's injury → 404 (no leak).

### Slice 3 — Exercise catalog + seed  (#21, #22, #26)
- `GET /exercises` (filters + pagination), `GET /exercises/{id}`.
- Seed script: muscles, equipment, exercises, contraindications.
- Frontend: exercise browser.
- **Contract tests:** filter intersection correct; `page_size` capped; unknown id → 404.

### Slice 4 — Manual workouts  (#23, #27)
- `POST/GET/PATCH/DELETE /workouts` (manual; `source = "manual"`).
- Frontend: manual workout builder.
- **Contract tests:** non-existent `exercise_id` → 422; another user's workout → 404.

---

## 6. Slice 0 in detail

### 6.1 Hexagonal folders
Create the `http/ app/ domain/ ports/ adapters/db/ config/` tree under `Server/src/`. Initially mostly
empty (populated slice by slice). `main.cpp` becomes the **composition root** — the one place that
wires concrete adapters into the ports. Keeps the core testable and framework-agnostic.

### 6.2 Wire argon2 + jwt-cpp
- **argon2:** link the system `libargon2` (MSYS2: `mingw-w64-ucrt-x86_64-argon2`; Ubuntu:
  `libargon2-dev` / runtime `libargon2-1`). Add to CI + Dockerfile.
- **jwt-cpp:** header-only, pulled via CMake `FetchContent` (pinned tag), like libpqxx. Needs a JSON
  traits backend — we already use nlohmann-json.
- **Verify on both platforms** before building on top (the libpqxx lesson): a tiny smoke test that
  hashes a password and mints+verifies a token, run locally and in CI.

### 6.3 Startup migration runner
A small C++ component that, on boot: ensures `schema_migrations` exists, lists `migrations/*.sql`
sorted by number, and executes any whose version isn't recorded yet — each wrapped in a transaction,
recording its version on success. Replaces reliance on Postgres's one-time `docker-entrypoint-initdb.d`
so migrations also apply to already-existing databases and in dev.

### 6.4 Integration-test DB harness
A GoogleTest fixture that connects to a dedicated **test database**, runs migrations once, and wraps
**each test in a transaction that is rolled back** at teardown — so tests are isolated and fast. Keeps
DB integration tests separate from pure unit tests (which use fakes, no DB).

### 6.5 Full v1 schema migration
Translate `01-data-model.md` into `Server/migrations/0002_core_schema.sql` (users + profile,
injuries, muscles, equipment, exercises + join tables, contraindications, workouts,
workout_exercises, generation_requests, strength norms). Applied by the new runner.

---

## 7. TDD rhythm (per endpoint)

**Red → Green → Refactor**, driven by `02-api-contract.md`:
1. Pick an endpoint; copy its listed test cases into a test file (they fail — nothing exists yet).
2. Implement the port + minimal logic (unit tests use the in-memory fake).
3. Add the Postgres adapter (integration tests use the test-DB harness).
4. Wire the controller; contract test boots the app and asserts the real HTTP behavior.
5. Refactor with tests green.

---

## 8. Issue map (GitHub milestone #3)

| Issue | Slice |
|---|---|
| #15 Migrations for all v1 tables | 0 |
| #16 Repository ports + in-memory fakes | 1 |
| #17 Password hashing (argon2) + JWT + auth middleware | 1 |
| #18 Auth endpoints: register/login/me | 1 |
| #19 Profile endpoints: GET/PATCH /users/me | 2 |
| #20 Injuries CRUD | 2 |
| #21 Exercise catalog: GET /exercises + /{id} | 3 |
| #22 Seed script | 3 |
| #23 Manual workouts CRUD | 4 |
| #24 Frontend: auth flow | 1 |
| #25 Frontend: profile + injuries forms | 2 |
| #26 Frontend: exercise browser | 3 |
| #27 Frontend: manual workout builder | 4 |
