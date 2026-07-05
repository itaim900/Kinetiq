# Fit App — context brief (for humans & AI agents)

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
(separate `fitapp_test` DB + per-test transaction rollback). `ctest` = **9/9** locally and in CI
(integration tests run against a Postgres service in the CI backend job). **Slice 1 (Auth) is next.**
See "Phase 2 plan" below and `docs/phase-2-plan.md`.

## Machine constraints (important)
- **8 GB RAM.** Do NOT run a heavy C++ build and Docker simultaneously (caused OOM crashes). Building
  libpqxx from source locally in Docker also OOM'd — that compile only runs in CI.
- Docker Postgres runs on the **default port 5432**. A native **PostgreSQL 16 Windows service**
  (`postgresql-x64-16` + `pemAgent`) used to occupy 5432; both were set to **Manual** start to free
  the port (reversible — see `docs/learning/how-to-run.md`).
- Backend commands need `C:\msys64\ucrt64\bin` on PATH (run from `Server/`). `vcpkg` exists at `C:\Users\<user>\vcpkg`
  but is **unused** (abandoned — see below).
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
  mismatch that breaks linking (even on C++17). CMake option **`FITAPP_FETCH_LIBPQXX=ON`** builds
  libpqxx from source (pinned **7.9.2**) for CI + Docker; local MSYS2 keeps the prebuilt (OFF).
- **Images built in CI, not locally** — host only pulls + runs. Backend **`BIND_ADDR`** defaults to
  `127.0.0.1` (safe local dev); container sets `0.0.0.0` so the frontend nginx can reach it.
  **`DB_PASSWORD` is NOT baked into the image** — supplied at runtime by compose.
- **`FITAPP_BUILD_TESTS=OFF`** in the Docker image build (no GoogleTest in the image); ON for local
  dev and CI.

## Container / CI layout
| File | Purpose |
|---|---|
| `docker-compose.yml` | Dev: Postgres only (host port 5432) |
| `docker-compose.prod.yml` | Prod-like: postgres + backend + client containers (app on **localhost:3000**) |
| `Server/Dockerfile` | Multi-stage: Ubuntu build (FetchContent libpqxx) → slim runtime |
| `Client/Dockerfile` + `nginx.conf` | Multi-stage: Node build → nginx serves static + proxies `/api` → `backend:8080` |
| `.github/workflows/ci.yml` | Build + test backend (with `FITAPP_FETCH_LIBPQXX=ON`; **Postgres service** for integration tests) and frontend |
| `.github/workflows/images.yml` | Build + push to GHCR on push to `main` |

Migrations are **shipped inside the backend image** (`/app/migrations`, `MIGRATIONS_DIR`) and applied
by the server on startup; the Postgres `docker-entrypoint-initdb.d` mount was removed from compose.

**Folder layout:** repo uses `Server/` (C++ API) and `Client/` (React SPA). Docker compose service
names and GHCR image names still use `backend` / `frontend` for compatibility:
`ghcr.io/itaim900/fit-app-backend:latest` and `ghcr.io/itaim900/fit-app-frontend:latest` (also tagged
by commit SHA). Repo is **private**; **packages are PUBLIC** (pull needs no login).

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
- **Slice 1 — Auth (NEXT):** #16 repo ports + fakes, #17 argon2+JWT+middleware, #18 register/login/me, #24 FE auth.
- **Slice 2 — Profile & Injuries:** #19 `/users/me`, #20 injuries CRUD, #25 FE forms (RHF+Zod).
- **Slice 3 — Exercises + seed:** #21 catalog endpoints, #22 seed script, #26 FE browser.
- **Slice 4 — Manual workouts:** #23 workouts CRUD, #27 FE builder.

**TDD rhythm:** write failing test from the test cases in `02-api-contract.md` → implement against
port/fake → green → refactor.

## Troubleshooting (learned this session)
| Symptom | Likely cause |
|---|---|
| **Database: unavailable** | Postgres container not running, Docker engine frozen, or backend
  `DB_PORT` mismatch (was 5432 vs 5433 when native Postgres held 5432 — now fixed on default 5432). |
| `docker` commands hang | Docker Desktop open but engine not ready — restart Docker Desktop / WSL. |
| CI backend link error on Linux | Fixed by `FITAPP_FETCH_LIBPQXX=ON` (don't use Ubuntu `libpqxx-dev`). |
| Local Docker build OOM | Expected on 8 GB — use CI-built images instead. |

## Docs map (`docs/`)
- `01-data-model.md` — DB schema + ER diagram
- `02-api-contract.md` — REST API contract (contract-first, test cases)
- `03-llm-port.md` — AI subsystem, `ILLMClient`, validation, weight engine
- `04-architecture.md` — system architecture, folder layout
- `05-roadmap.md` — phased plan + definitions of done (Phase 1 complete)
- `phase-1-summary.md` — Phase 1 recap (file inventory, decisions, verification)
- `phase-2-plan.md` — Phase 2 plan (slices, locked decisions, TDD workflow)
- `adr/` — decision records (note: 0003 & 0005 superseded by 0011 & 0012)
- `learning/` — git-ignored personal notes: `code-walkthrough.md`, `how-to-run.md`,
  `session-log-phase1-spike.md`

## GitHub
Repo `itaim900/Fit-App` (private). Backlog = **4 milestones** (Phase 0–3) + **38 issues** + labels.
Actions: https://github.com/itaim900/Fit-App/actions

## How to run
See `docs/learning/how-to-run.md`. Two ways:
- **Dev (fast iteration):** `docker compose up -d` (Postgres) → build & run server
  (`cmake --preset default && cmake --build --preset default`, then `./build/fitapp_server.exe`
  from `Server/`) → `npm run dev` in `Client/` → open **localhost:5173**.
- **Whole app via containers:** `docker compose -f docker-compose.prod.yml pull && up -d` → open
  **localhost:3000**. Requires GHCR packages public, or `docker login ghcr.io` with `read:packages`.

**Ports:** 5173 (Vite dev), 8080 (backend), 5432 (Postgres dev), 3000 (containerized app UI).

## Next steps
**Phase 2, Slice 1 — Auth** (see `docs/phase-2-plan.md`), TDD per `02-api-contract.md`:
1. **#16** `IUserRepository` port + in-memory fake (unit-testable) + `PgUserRepository` (integration).
2. **#17** argon2 password hashing service + JWT issue/verify service + Crow auth **middleware**
   (`Authorization: Bearer` → `CurrentUser`); promote auth deps from the test target onto
   `fitapp_server` and add `libargon2`/`libssl` runtime libs to `Server/Dockerfile`.
3. **#18** `POST /auth/register`, `POST /auth/login`, `GET /users/me` (controllers + DTOs).
4. **#24** Client auth (login/register forms, token storage, authed fetch).

Notes: the auth libs currently link into `fitapp_tests` only (Slice 0) — Slice 1 moves them to the
server + image. Consider capturing the 4 Phase 2 locked decisions as ADRs (0013–00xx).
