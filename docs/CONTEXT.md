# Kinetiq — context brief (for humans & AI agents)

Dense summary to bring a new agent up to speed cheaply. For detail, follow the linked docs.

## What it is
AI-powered personalized workout-plan web app. Purpose: **learning + portfolio**. Owner is a
**beginner** at C++ and React — prefer clear explanations, small steps, and planning over fast code.

## Stack (as built)
- **Server (C++):** C++20, **Crow** (header-only HTTP, vendored at
  `Server/third_party/crow/crow_all.h`), **libpqxx** (Postgres), JSON via nlohmann-json.
  Build: **CMake + Ninja** on **MSYS2 UCRT64 GCC** (preset `default`). Tests: **GoogleTest**.
- **Client (React):** React 18 + **TypeScript** + **Vite**. Tests: **Vitest + Testing Library + MSW**.
  Dev server proxies `/api` → server.
- **Database:** PostgreSQL 16 via **Docker Compose**. Migrations in `Server/migrations/` (SQL,
  numbered `NNNN_*.sql`; applied by the **C++ startup migration runner**, not initdb).
- **Auth deps (Phase 2):** **argon2** (password hashing, system lib) + **jwt-cpp** (JWT, header-only
  via CMake FetchContent) + **OpenSSL** (libcrypto, used by jwt-cpp). All prebuilt on both platforms.

## Architecture (intended)
Hexagonal / ports & adapters (ADR-0002). HTTP layer is a thin adapter; core never imports Crow.
Persistence behind repository ports. AI = **hybrid**: deterministic C++ (safety filter, calorie
MET calc, starting-weight engine) + LLM (Gemini behind `ILLMClient` port) for creative structuring
only, with schema validation + self-correction. Auth: short-lived JWT (v1).

## Status
**Phase 1 "walking skeleton" COMPLETE & fully verified (2026-07-05).** End-to-end: React → Vite proxy
→ Crow backend → libpqxx → Postgres. `GET /api/v1/health` → `{"status":"ok","db":"connected"}`.
Merged to `main` (PR #39); all 9 Phase 1 issues closed. See `docs/phase-1-summary.md`.

**CI green** — `.github/workflows/ci.yml` builds + tests both halves on Ubuntu on every push/PR.
**Containerized & verified** — multi-stage Dockerfiles; images published to **GHCR** (now **public**)
by `.github/workflows/images.yml`; full stack via `docker-compose.prod.yml`. Verified by an
**anonymous** `pull` + `up` → app served at `localhost:3000`, DB connected. Images build in CI
(not on the 8 GB host).

**Phase 2 (Core Domain) IN PROGRESS. Slice 0 "Foundations" COMPLETE & verified (2026-07-05, PR #40
merged to `main`).** Hexagonal folders, argon2 + jwt-cpp wired (smoke tests green on MSYS2 + CI),
startup migration runner, full v1 schema (`0002_core_schema.sql`), integration-test DB harness
(separate `kinetiq_test` DB + per-test transaction rollback). **In-process HTTP test harness**
(`tests/support/HttpClient.h`) added; testing architecture documented in `phase-2-plan.md` §8.

**Slice 1 (Auth) IN PROGRESS (started 2026-07-12).** Issue #16 half-done: `domain::User` +
`ports::IUserRepository` (+ `DuplicateEmailError`) + `tests/support/InMemoryUserRepository`
fake, all TDD'd (red confirmed via failed compile, then green). `domain::PasswordPolicy` done:
**min 8 chars + at least one uppercase letter** (superseded an earlier length-only/10-char
version same day). Scope also grew to include `PATCH /auth/password` (change password while
logged in — no email infra needed, unlike the deferred reset flow). Full decision log + test
case list in `docs/02-api-contract.md`; full step-by-step build log in
`docs/phase-2-slice-1-auth.md`. **Explicitly deferred** (2026-07-12, each needs its own future
design): Google OAuth, "forgot password" reset (needs email infra), account deletion (needs
re-auth + cascade-delete design), login rate-limiting, email verification.

**SLICE 1 (AUTH) COMPLETE (2026-07-16) — `#16`, `#17`, `#18`, `#24` all done.** `#16`:
`db::PgUserRepository` (real Postgres adapter), tested via `DbTest`. `#17`:
`IPasswordHasher`/`Argon2PasswordHasher` (real argon2id, random salt per call),
`ITokenService`/`JwtTokenService` (HS256, **24h expiry** — locked decision, no refresh tokens in
v1 so this is the actual session length), and the `authenticate()` function in
`http/middleware/Authenticator.h` (deliberately **not** Crow middleware — `HttpClient`'s
`handle_full()` doesn't run it). `#18`: `app/` services (`RegisterUser`, `LoginUser`,
`ChangePassword`), `http/dto/AuthDto.h` + `http/controllers/AuthController` (all 4 endpoints:
register/login/me/change-password), a production-only `AutoCommitPgUserRepository` (opens a fresh
connection+transaction per call — a shared long-lived transaction isn't safe across a
multithreaded server's concurrent requests), and `main.cpp` fully wired (incl. a `JWT_SECRET` env
var with a dev fallback that logs a warning when used). **Live-verified manually** against the
real server + Postgres, not just tests: full register → login → me → change-password flow via
curl, all correct. Auth deps promoted from `kinetiq_tests`-only onto `kinetiq_server` +
`Server/Dockerfile` (Dockerfile package names **not build-verified locally** — this project never
builds Docker images on the 8GB dev machine; CI is the real check). `#24`: Client auth UI —
`RegisterForm`/`LoginForm`/`AuthPage` (Vitest + Testing Library + MSW, 22 tests), session
restoration + logout in `App.tsx`, **`localStorage`** token persistence (not in-memory-only —
needed so the 24h token actually survives a page refresh), and a "Momentum" visual design (dark
ground, coral accent, breathing radial glow) for the login screen. Forgot-password is a
**visual-only placeholder** (shows "coming soon," no backend — reset flow is still deferred).
**Live-verified via browser automation** against the real server + Postgres, not just tests —
including catching one genuine layout bug (an `overflow: hidden` that made a link unreachable on
short viewports) via Playwright's actionability checks.
`ctest` = **70/70** locally. Dev stack re-verified end-to-end
(Postgres → Server → Client proxy → health OK).

## Machine constraints (important)
- **8 GB RAM.** Do NOT run a heavy C++ build and Docker simultaneously (caused OOM crashes). Building
  libpqxx from source locally in Docker also OOM'd — that compile only runs in CI.
- Docker Postgres runs on the **default port 5432**. A native **PostgreSQL 16 Windows service**
  (`postgresql-x64-16` + `pemAgent`) used to occupy 5432; both were set to **Manual** start to free
  the port (reversible — see `docs/learning/how-to-run.md`).
- Backend commands need `C:\msys64\ucrt64\bin` on PATH (run from `Server/`). `vcpkg` exists at `C:\Users\<user>\vcpkg`
  but is **unused** (abandoned — see below).
- **Launch server/client via `scripts/run-dev.ps1 -Target server|client`** (from project root),
  not the raw exe / `npm run dev` directly. Past sessions left orphaned dev-server processes
  running for days, squatting ports and wasting RAM — the wrapper binds the child process tree to
  a Windows Job Object (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) so it's killed automatically if the
  wrapper process ever ends, even via a forced kill. Verified through 4 levels of process nesting.
- **Git hooks** in `.githooks/` (enable: `git config core.hooksPath .githooks`): pre-commit checks the
  changed half, pre-push runs everything and blocks if DB tests skipped. Spec:
  `docs/superpowers/specs/2026-10-21-git-hooks-design.md`.
- **Repo folder was renamed `Fit App` → `Kinetiq`.** Compose derives the project name from the
  folder, so the existing Postgres container/volume belong to project `fitapp`
  (`fitapp_kinetiq_pgdata`) and `docker compose up -d` now clashes on the container name. Until
  that's fixed, start Postgres with **`docker start kinetiq-postgres`**.
- Docker Desktop's built-in **Kubernetes was turned off** (2026-10-21) to save RAM; settings backup
  at `%APPDATA%\Docker\settings-store.json.bak-2026-10-21`.
- **GitHub CLI (`gh`)** installed at `C:\Program Files\GitHub CLI\gh.exe` (v2.95.0), logged in as
  `itaim900`. On system PATH; restart terminal/Cursor if `gh` not found. Token scopes: `repo`,
  `read:org`, `gist` (no `read:packages` unless refreshed).

## Key decisions / history
- Originally chose MSVC + vcpkg + Drogon; building Drogon from source OOM'd on 8 GB. **Pivoted** to
  MSYS2 + Crow. Recorded as **ADR-0011 (Crow, supersedes 0003)** and **ADR-0012 (MSYS2, supersedes
  0005)**.
- Postgres stays in Docker (ADR-0004). Native MSYS2 Postgres was broken by a `pacman -Sy` partial
  upgrade — always use `pacman -Syu`.
- **Cross-platform linking:** Windows socket libs (`ws2_32`, etc.) linked only when `WIN32` in
  `CMakeLists.txt` so CI/Docker (Linux) builds succeed.
- **libpqxx on Linux:** Ubuntu's `libpqxx-dev` 7.8.1 has a header/lib `std::source_location`
  mismatch that breaks linking (even on C++17). CMake option **`KINETIQ_FETCH_LIBPQXX=ON`** builds
  libpqxx from source (pinned **7.9.2**) for CI + Docker; local MSYS2 keeps the prebuilt (OFF).
- **Images built in CI, not locally** — host only pulls + runs. Backend **`BIND_ADDR`** defaults to
  `127.0.0.1` (safe local dev); container sets `0.0.0.0` so the frontend nginx can reach it.
  **`DB_PASSWORD` is NOT baked into the image** — supplied at runtime by compose.
- **`KINETIQ_BUILD_TESTS=OFF`** in the Docker image build (no GoogleTest in the image); ON for local
  dev and CI.

## Container / CI layout
| File | Purpose |
|---|---|
| `docker-compose.yml` | Dev: Postgres only (host port 5432) |
| `docker-compose.prod.yml` | Prod-like: postgres + backend + client containers (app on **localhost:3000**) |
| `Server/Dockerfile` | Multi-stage: Ubuntu build (FetchContent libpqxx) → slim runtime |
| `Client/Dockerfile` + `nginx.conf` | Multi-stage: Node build → nginx serves static + proxies `/api` → `backend:8080` |
| `.github/workflows/ci.yml` | Build + test backend (with `KINETIQ_FETCH_LIBPQXX=ON`; **Postgres service** for integration tests) and frontend |
| `.github/workflows/images.yml` | Build + push to GHCR on push to `main` |

Migrations are **shipped inside the backend image** (`/app/migrations`, `MIGRATIONS_DIR`) and applied
by the server on startup; the Postgres `docker-entrypoint-initdb.d` mount was removed from compose.

**Folder layout (important):** all source code lives in **`Server/`** (C++ API) and **`Client/`**
(React SPA). There are **no** `backend/` or `frontend/` folders — those were renamed in PR #39.
The words "backend" / "frontend" still appear elsewhere as **role names**, not folder paths:
- Docker Compose **service names** (`backend`, `frontend`) — how containers talk on the network
  (e.g. nginx proxies to `http://backend:8080`).
- **GHCR image names** (`kinetiq-backend`, `kinetiq-frontend`) — already published; kept for compatibility.
- **CI job labels** and prose in docs ("build the backend").
Do **not** recreate `backend/` or `frontend/` directories; put new code in `Server/` or `Client/`.
Repo is **private**; **packages are PUBLIC** (pull needs no login).

## Phase 2 plan (Core Domain) — in progress
Goal: non-AI features, **test-first**: register/login, profile + injuries, exercise catalog, manual
workouts. Full detail in `docs/phase-2-plan.md`. Built as vertical slices (DB → port → logic →
endpoint → UI), **auth first** (every other endpoint is 🔒).

**Locked decisions:**
1. **Migrations:** C++ **startup migration runner** — applies `migrations/NNNN_*.sql` not yet in
   `schema_migrations`, in order (dev + container consistent).
2. **Integration-test DB:** dedicated **test database** + **per-test transaction rollback** (fast, isolated).
3. **Libraries:** **argon2** (password hashing; `libargon2`/`libargon2-dev`, prebuilt both platforms)
   + **jwt-cpp** (JWT; header-only via CMake FetchContent, ABI-safe).
4. **Schema:** write the **full v1 schema up front** (issue #15) from `01-data-model.md`.

**Slices (with issues):**
- **Slice 0 — Foundations: ✅ DONE (PR #40).** Hexagonal folders (`http/ app/ domain/ ports/
  adapters/db/ config/`, each with a role README), argon2 + jwt-cpp wired + smoke-tested (MSYS2 + CI),
  `Migrator` startup runner (owns `schema_migrations`, fails closed), `DbTest` harness, full v1 schema
  (`0002_core_schema.sql`).
- **Slice 1 — Auth: ✅ DONE (2026-07-16).** #16 repo ports + fakes ✅, #17 argon2+JWT+
  **authenticator fn** ✅ (not Crow middleware — see Testing architecture), #18
  register/login/me/**change-password** ✅, #24 Client auth ✅ (`RegisterForm`/`LoginForm`/
  `AuthPage`, `localStorage` token, "Momentum" visual design). All live-verified against the real
  server/DB, backend via curl and frontend via browser automation. Password policy (min 8 chars +
  ≥1 uppercase) and login error shape (generic 401, no email-enumeration) locked in
  `docs/02-api-contract.md`. **Explicitly deferred to future slices:** Google OAuth,
  forgot-password reset (Client has a visual-only placeholder), account deletion, login
  rate-limiting, email verification.
- **Slice 2 — Profile & Injuries: IN PROGRESS (started 2026-07-25, branch `slice-2-profile`).**
  Design brainstormed (8 locked decisions, see spec) + Stage A TDD plan written.
  **Stage A (#19 `GET`/`PATCH /users/me`) ✅ DONE:** grew `domain::User` + `domain::ProfilePolicy`
  validator, `PgUserRepository` maps/persists all profile columns (no migration — Slice 0 built
  them), `app::UpdateProfile` (load-modify-save), `ProfileDto` + a new `UserController` (GET/PATCH
  on one Crow multi-method route, RFC 9457 field errors), wired into `main.cpp`.
  **Stage B (#20 injuries CRUD) ✅ DONE:** full new vertical stack — `domain::Injury` +
  `InjuryPolicy`, `IInjuryRepository` + fake, `PgInjuryRepository` + `AutoCommitPgInjuryRepository`,
  one cohesive `app::InjuryService` (shared ownership check → **404** for another user's injury),
  `InjuryDto` + `InjuryController` (collection GET/POST + item PATCH/DELETE by `<int>` id), wired
  into `main.cpp`. `ctest` **122/122**, live-verified via curl. Still to do: #25 FE forms (RHF+Zod).
- **Slice 3 — Exercises + seed:** #21 catalog endpoints, #22 seed script, #26 FE browser.
- **Slice 4 — Manual workouts:** #23 workouts CRUD, #27 FE builder.

**TDD rhythm:** write failing test from the test cases in `02-api-contract.md` → implement against
port/fake → green → refactor.

**Testing architecture** (see `phase-2-plan.md` §8):
- **Pyramid:** domain unit → service unit (in-memory fakes) → adapter integration (`DbTest`) → HTTP
  in-process (`HttpClient`) → Client (Vitest + MSW).
- **Fakes:** hand-written in-memory repo implementations under `tests/support/` (no gmock).
- **HTTP tests:** `HttpClient` drives Crow via `app.handle_full()` — real routing/handlers, no socket.
- **Crow middleware caveat:** `handle_full()` does **not** run middleware (only the socket path does).
  Auth for Slice 1 = an **authenticator function** handlers call, not Crow middleware — keeps protected
  routes testable in-process. Will become an ADR.

## Troubleshooting (learned this session)
| Symptom | Likely cause |
|---|---|
| **Database: unavailable** | Postgres container not running, Docker engine frozen, or Server
  `DB_PORT` mismatch (was 5432 vs 5433 when native Postgres held 5432 — now fixed on default 5432). |
| Server exits: `migrations failed, refusing to start` | Postgres not up yet — start `docker compose up -d`, wait for `healthy`, rerun Server. |
| `docker` commands hang | Docker Desktop open but engine not ready — restart Docker Desktop / WSL. |
| CI backend link error on Linux | Fixed by `KINETIQ_FETCH_LIBPQXX=ON` (don't use Ubuntu `libpqxx-dev`). |
| Local Docker build OOM | Expected on 8 GB — use CI-built images instead. |

## Docs map (`docs/`)
- `01-data-model.md` — DB schema + ER diagram
- `02-api-contract.md` — REST API contract (contract-first, test cases)
- `03-llm-port.md` — AI subsystem, `ILLMClient`, validation, weight engine
- `04-architecture.md` — system architecture, folder layout
- `05-roadmap.md` — phased plan + definitions of done (Phase 1 complete)
- `phase-1-summary.md` — Phase 1 recap (file inventory, decisions, verification)
- `phase-2-plan.md` — Phase 2 plan (slices, locked decisions, TDD workflow)
- `phase-2-slice-1-auth.md` — Slice 1 (Auth) step-by-step build log: every decision and file, in order
- `phase-2-slice-2-profile-injuries.md` — Slice 2 (Profile & Injuries) living build log
- `superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md` — Slice 2 design spec (8 decisions)
- `superpowers/plans/2026-07-17-slice-2a-profile-backend.md` — Slice 2 Stage A bite-sized TDD plan
- `superpowers/plans/2026-07-25-slice-2b-injuries-backend.md` — Slice 2 Stage B bite-sized TDD plan
- `superpowers/specs/2026-07-25-injury-muscle-link-design.md` — injury/muscle link spec (E1–E6; partly superseded)
- `superpowers/plans/2026-07-25-injury-muscle-link.md` — its plan (Tasks 1–2 done; 3–8 superseded)
- `superpowers/specs/2026-10-21-injury-conditions-catalog-design.md` — multi-muscle injuries + conditions catalog (C1–C9, ICD-11 sources)
- `superpowers/plans/2026-10-21-injury-conditions-catalog.md` — its 6-task TDD plan (done)
- `superpowers/specs/2026-10-21-git-hooks-design.md` + `plans/2026-10-21-git-hooks.md` — local git hooks
- `adr/` — decision records (note: 0003 & 0005 superseded by 0011 & 0012)
- `learning/` — git-ignored personal notes: `code-walkthrough.md`, `how-to-run.md`,
  `session-log-phase1-spike.md`

## GitHub
Repo `itaim900/KinetiQ` (private). Backlog = **4 milestones** (Phase 0–3) + **38 issues** + labels.
Actions: https://github.com/itaim900/KinetiQ/actions

## How to run
See `docs/learning/how-to-run.md`. Two ways:
- **Dev (fast iteration):** `docker compose up -d` (Postgres) → build & run server
  (`cmake --preset default && cmake --build --preset default`, then `./build/kinetiq_server.exe`
  from `Server/`) → `npm run dev` in `Client/` → open **localhost:5173**.
- **Whole app via containers:** `docker compose -f docker-compose.prod.yml pull && up -d` → open
  **localhost:3000**. Requires GHCR packages public, or `docker login ghcr.io` with `read:packages`.

**Ports:** 5173 (Vite dev), 8080 (backend), 5432 (Postgres dev), 3000 (containerized app UI).

## Next steps
**Phase 2, Slice 2 — Profile & Injuries: IN PROGRESS** on branch `slice-2-profile`. Full narrative
in `docs/phase-2-slice-2-profile-injuries.md`; design in the spec (8 locked decisions).

- **Stage A — #19 `GET`/`PATCH /users/me`: ✅ DONE** (2026-07-25). `ctest` 94/94, live-verified.
- **Stage B — #20 Injuries CRUD (`/users/me/injuries`): ✅ DONE** (2026-07-25). Full new vertical
  stack; ownership check returns **404** for another user's injury. `ctest` 122/122, live-verified.
  Drives the Phase 3 rule-based safety filter.
- **Stage B extension — Injury conditions catalog: ✅ DONE** (2026-10-21). Injuries affect
  **several muscles**. A catalog of 17 known conditions (ICD-11-anchored; migration `0004`) maps
  each to muscles with a role (primary/secondary). **Known condition = locked** (region, type,
  muscles from the catalog; sending them → 422 naming the field); **unknown injury** = user picks
  region (fixed list of 14, FK-enforced), type, muscles (`user_injury_muscles`, migration `0005`).
  New read-only `GET /body-regions`, `/muscles`, `/injury-conditions`. `kinetiq_tests` **171/171,
  0 skipped**; live-verified. Spec `superpowers/specs/2026-10-21-injury-conditions-catalog-design.md`
  (supersedes July E2/E4/E5), plan `superpowers/plans/2026-10-21-injury-conditions-catalog.md`.
  Secondary-muscle mappings are project judgement, pending physio review.
- **Stage C — #25 Frontend: IN PROGRESS (Tasks 1–3 of 9 done, 2026-10-22).** Designed with browser
  mockups: top bar with Injuries/Profile tabs, one searchable condition field ("My injury isn't
  listed…" → region/type/muscles), inline forms, not-medical-advice notice. Spec
  `superpowers/specs/2026-10-22-stage-c-frontend-design.md`, plan
  `superpowers/plans/2026-10-22-stage-c-frontend.md`. Done: T1 rename **KinetiQ → Kinetiq**
  (display text; GitHub repo links unchanged), T2 libs (react-router 7.18.4, react-hook-form
  7.90.0, zod 3.25.76, @hookform/resolvers 5.9.2, exact) + `authFetch`/`ApiError`/401 handler +
  typed API + stateful MSW (`resetMockDb`), T3 `AppShell` + routing + session-expired logout
  (pages are stubs). Client tests 31/31. **Next session starts at Task 4** (injury cards list).
  Then merge `slice-2-profile` → `main` after Task 9 (owner's go-ahead).

**Carry-overs worth remembering:**
- Reuse the `AutoCommitPgUserRepository` pattern (connection-per-call, not one shared long-lived
  transaction) for the new `PgInjuryRepository` wired into `main.cpp`'s routes.
- When a build's success gates the next command, check the real exit code (`; echo exit=$?`), not a
  piped `tail` — a pipeline reports the last command's status, masking build failures.
- Merge `slice-2-profile` → `main` when the whole slice (A+B+C) is complete and verified.

Capture Phase 2 locked decisions + auth-as-authenticator as ADRs (0013–00xx); Slice 2's D1
(expand-User) and D2 (load-modify-save) are candidates too.
