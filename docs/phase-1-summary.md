# Phase 1 Summary — Walking Skeleton

**Status:** ✅ COMPLETE & verified end-to-end (2026-07-05)
**Milestone:** Phase 1 — Walking Skeleton
**Goal:** Prove the entire toolchain end-to-end (React → C++ → Postgres) with no business logic,
so future phases are just "filling in." Then make it portable (CI + containers).

---

## 1. What Phase 1 delivered

A thin but complete vertical slice that exercises every layer, plus the infrastructure to build,
test, and run it anywhere:

- **Backend (C++/Crow)** exposing `GET /api/v1/health`, which checks its own status **and** the
  database connection, returning `{"status":"ok","db":"connected"}`.
- **Frontend (React/Vite)** that calls that endpoint and renders the live status.
- **Database (PostgreSQL 16)** in Docker, with a migration system and a first baseline migration.
- **Tests** wired on both sides (GoogleTest for C++, Vitest + Testing Library + MSW for React).
- **CI** (GitHub Actions) that builds and tests both halves on a clean Ubuntu machine on every push.
- **Containers** — multi-stage Docker images for server + client, published to **GHCR**, runnable
  as a whole app with a single `docker compose` command on any machine with Docker.

**Definition of Done — met:** a developer can clone the repo and either (a) run the dev stack, or
(b) `docker compose -f docker-compose.prod.yml up` and see the React page show live data from the C++
backend backed by Postgres; **CI is green**.

---

## 2. End-to-end verification (evidence)

Final acceptance test, run 2026-07-05 (images pulled **anonymously** from public GHCR — the true
"any machine" test):

| Check | Result |
|---|---|
| `docker compose -f docker-compose.prod.yml pull` (no login) | ✅ all 3 images pulled |
| All containers up | ✅ postgres (healthy), backend, frontend |
| `http://localhost:3000/api/v1/health` (via nginx proxy) | ✅ `{"db":"connected","status":"ok"}` |
| `http://localhost:8080/api/v1/health` (backend direct) | ✅ `{"db":"connected","status":"ok"}` |
| `http://localhost:3000/` (SPA served) | ✅ page title "Fit App" |
| CI on `main` | ✅ green (backend + frontend) |
| Publish images on `main` | ✅ both images built & pushed |

---

## 3. File inventory (what each file is for)

### Backend — `Server/`
| File | Purpose |
|---|---|
| `src/main.cpp` | Crow app; defines `GET /api/v1/health`; connects to Postgres via libpqxx; `BIND_ADDR` env controls bind address (127.0.0.1 local / 0.0.0.0 in container) |
| `src/config/Env.h` | Header-only `envOr(key, fallback)` helper for reading env vars (pure + unit-testable) |
| `tests/test_main.cpp` | GoogleTest unit tests for `Env.h` |
| `migrations/0001_init.sql` | Baseline migration: `citext` extension + `schema_migrations` bookkeeping table |
| `CMakeLists.txt` | Build recipe. Options: `FITAPP_FETCH_LIBPQXX` (build libpqxx from source on Linux), `FITAPP_BUILD_TESTS` (skip tests in image). Windows socket libs guarded by `if(WIN32)` |
| `CMakePresets.json` | `default` preset: Ninja generator, MSYS2 GCC |
| `third_party/crow/crow_all.h` | Vendored single-header Crow web framework |
| `Dockerfile` | Multi-stage: Ubuntu build (compiles libpqxx + server) → slim runtime image |
| `.env.example` | Template for DB connection vars + `GEMINI_API_KEY` |

### Frontend — `Client/`
| File | Purpose |
|---|---|
| `src/main.tsx` | React entry point |
| `src/App.tsx` | Main component; fetches health and renders status |
| `src/api.ts` | `fetchHealth()` — network call abstraction |
| `src/App.test.tsx` | Vitest test rendering `App` against a mocked API |
| `src/mocks/handlers.ts`, `src/mocks/server.ts` | MSW mock server + handlers |
| `src/test/setup.ts` | Vitest/MSW setup |
| `vite.config.ts` | Vite config + dev proxy (`/api` → `127.0.0.1:8080`) + Vitest config |
| `Dockerfile` | Multi-stage: Node builds static files → nginx serves them |
| `nginx.conf` | Serves the SPA + proxies `/api` → `backend:8080` (prod equivalent of the Vite proxy) |
| `package.json` | Scripts (`dev`, `build`, `test`) + deps |

### Root / infra
| File | Purpose |
|---|---|
| `docker-compose.yml` | **Dev**: runs Postgres only (host port 5432) so you run server/client natively with hot reload |
| `docker-compose.prod.yml` | **Whole app**: postgres + backend + frontend from GHCR images; app on `localhost:3000` |
| `.dockerignore` | Keeps the backend image build context lean |
| `.github/workflows/ci.yml` | Build + test backend (C++/GoogleTest) and frontend (Vitest) on Ubuntu |
| `.github/workflows/images.yml` | Build + push both images to GHCR on push to `main` |

---

## 4. Key decisions & pivots (the interesting part)

Phase 1 involved several forced pivots, all driven by an **8 GB RAM** dev machine. Each is recorded
as an ADR.

| Decision | Why | Record |
|---|---|---|
| **Drogon → Crow** (web framework) | Drogon isn't packaged prebuilt in MSYS2; building it + OpenSSL + libpq from source OOM'd on 8 GB | ADR-0011 (supersedes 0003) |
| **vcpkg → MSYS2** (toolchain) | vcpkg's source builds were too heavy; MSYS2 offers fast prebuilt packages | ADR-0012 (supersedes 0005) |
| **Native Postgres → Docker Postgres** | MSYS2 native Postgres broke with a DLL/ICU conflict after a partial `pacman -Sy` | ADR-0004 |
| **Cross-platform build** | Windows socket libs (`ws2_32`, etc.) don't exist on Linux → guard with `if(WIN32)` | in `CMakeLists.txt` |
| **libpqxx from source on Linux** | Ubuntu's `libpqxx-dev` 7.8.1 has a header/lib `std::source_location` mismatch that breaks linking | `FITAPP_FETCH_LIBPQXX` option |
| **Images built in CI, not locally** | Building libpqxx from source OOMs on 8 GB — GitHub runners (~16 GB) do it instead; the dev machine only pulls & runs | `images.yml` |
| **`backend/`→`Server/`, `frontend/`→`Client/`** | Clearer naming (service/image names kept as `backend`/`frontend` for compatibility) | PR #39 |

---

## 5. How to run (recap)

**Dev (fast iteration):**
```powershell
docker compose up -d                         # Postgres only
# in Server/:  cmake --preset default && cmake --build --preset default && ./build/fitapp_server.exe
# in Client/:  npm run dev                    # http://localhost:5173
```

**Whole app (any machine, only Docker needed):**
```powershell
docker compose -f docker-compose.prod.yml pull
docker compose -f docker-compose.prod.yml up -d
# open http://localhost:3000
```

Full runbook: `docs/learning/how-to-run.md`.

---

## 6. Tech stack (as built)

- **Backend:** C++20, Crow (header-only HTTP), libpqxx (Postgres), nlohmann-json. Build: CMake +
  Ninja on MSYS2 UCRT64 GCC. Tests: GoogleTest.
- **Frontend:** React 18 + TypeScript + Vite. Tests: Vitest + Testing Library + MSW.
- **Database:** PostgreSQL 16 (Docker). Numbered SQL migrations auto-applied on first init.
- **CI/CD:** GitHub Actions (build+test + image publish). Images on GHCR.

---

## 7. Deliberately NOT in Phase 1

No business logic: no auth, no user profiles, no exercises, no workouts. The only endpoint is the
health check. All real features start in **Phase 2**.

---

## 8. Links

- Repo: https://github.com/itaim900/Fit-App
- Actions: https://github.com/itaim900/Fit-App/actions
- Images: `ghcr.io/itaim900/fit-app-backend`, `ghcr.io/itaim900/fit-app-frontend`
- Merge PR: #39
- Related docs: `01-data-model.md`, `02-api-contract.md`, `03-llm-port.md`, `04-architecture.md`,
  `05-roadmap.md`, `adr/`
