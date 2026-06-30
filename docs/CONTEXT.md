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
  numbered; auto-applied on first container init).

## Architecture (intended)
Hexagonal / ports & adapters (ADR-0002). HTTP layer is a thin adapter; core never imports Crow.
Persistence behind repository ports. AI = **hybrid**: deterministic C++ (safety filter, calorie
MET calc, starting-weight engine) + LLM (Gemini behind `ILLMClient` port) for creative structuring
only, with schema validation + self-correction. Auth: short-lived JWT (v1).

## Status
**Phase 1 "walking skeleton" COMPLETE.** End-to-end verified (dev mode): React → Vite proxy → Crow
backend → libpqxx → Postgres. `GET /api/v1/health` → `{"status":"ok","db":"connected"}`. Backend +
frontend test harnesses green. First migration (`0001_init`) applied. No business logic yet.

**CI green** — `.github/workflows/ci.yml` builds + tests both halves on Ubuntu on every push/PR.
**Containerized** — multi-stage Dockerfiles; images published to **GHCR** by
`.github/workflows/images.yml`; full stack via `docker-compose.prod.yml`. Images build in CI (not on
the 8 GB host). **Pending:** make GHCR packages public (user chose this) and verify
`docker compose -f docker-compose.prod.yml pull && up -d` end-to-end.

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
| `.github/workflows/ci.yml` | Build + test backend (with `FITAPP_FETCH_LIBPQXX=ON`) and frontend |
| `.github/workflows/images.yml` | Build + push to GHCR on push to `main` |

**Folder layout:** repo uses `Server/` (C++ API) and `Client/` (React SPA). Docker compose service
names and GHCR image names still use `backend` / `frontend` for compatibility:
`ghcr.io/itaim900/fit-app-backend:latest` and `ghcr.io/itaim900/fit-app-frontend:latest` (also tagged
by commit SHA). Repo is **private**; packages currently **private** — user intends to make them
**public** for portfolio pull-without-login.

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
1. Make GHCR packages public → verify full container stack locally.
2. **Phase 2** (auth, profile, exercise catalog, manual workouts) built test-first per
   `02-api-contract.md`.
