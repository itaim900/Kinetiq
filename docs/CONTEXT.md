# Fit App — context brief (for humans & AI agents)

Dense summary to bring a new agent up to speed cheaply. For detail, follow the linked docs.

## What it is
AI-powered personalized workout-plan web app. Purpose: **learning + portfolio**. Owner is a
**beginner** at C++ and React — prefer clear explanations, small steps, and planning over fast code.

## Stack (as built)
- **Backend:** C++20, **Crow** (header-only HTTP, vendored at `backend/third_party/crow/crow_all.h`),
  **libpqxx** (Postgres), JSON via nlohmann-json. Build: **CMake + Ninja** on **MSYS2 UCRT64 GCC**
  (preset `default`). Tests: **GoogleTest**.
- **Frontend:** React 18 + **TypeScript** + **Vite**. Tests: **Vitest + Testing Library + MSW**.
  Dev server proxies `/api` → backend.
- **Database:** PostgreSQL 16 via **Docker Compose**. Migrations in `backend/migrations/` (SQL,
  numbered; auto-applied on first container init).

## Architecture (intended)
Hexagonal / ports & adapters (ADR-0002). HTTP layer is a thin adapter; core never imports Crow.
Persistence behind repository ports. AI = **hybrid**: deterministic C++ (safety filter, calorie
MET calc, starting-weight engine) + LLM (Gemini behind `ILLMClient` port) for creative structuring
only, with schema validation + self-correction. Auth: short-lived JWT (v1).

## Status
**Phase 1 "walking skeleton" COMPLETE & verified end-to-end:** React → Vite proxy → Crow backend →
libpqxx → Postgres. `GET /api/v1/health` → `{"status":"ok","db":"connected"}`. Backend + frontend
test harnesses green. First migration applied. No features yet.

## Machine constraints (important)
- **8 GB RAM.** Do NOT run a heavy C++ build and Docker simultaneously (caused OOM crashes).
- Docker Postgres runs on the **default port 5432**. (A native PostgreSQL 16 Windows service that
  used to occupy 5432 was set to **Manual** start to free the port; reversible.)
- Backend commands need `C:\msys64\ucrt64\bin` on PATH. `vcpkg` exists at `C:\Users\<user>\vcpkg`
  but is **unused** (abandoned — see below).

## Key decisions / history
- Originally chose MSVC + vcpkg + Drogon; building Drogon from source OOM'd on 8 GB. **Pivoted** to
  MSYS2 + Crow. Recorded as **ADR-0011 (Crow, supersedes 0003)** and **ADR-0012 (MSYS2, supersedes
  0005)**.
- Postgres stays in Docker (ADR-0004). Native MSYS2 Postgres was broken by a `pacman -Sy` partial
  upgrade — always use `pacman -Syu`.

## Docs map (`docs/`)
- `01-data-model.md` — DB schema + ER diagram
- `02-api-contract.md` — REST API contract (contract-first, test cases)
- `03-llm-port.md` — AI subsystem, `ILLMClient`, validation, weight engine
- `04-architecture.md` — system architecture, folder layout
- `05-roadmap.md` — phased plan + definitions of done
- `adr/` — decision records (note: 0003 & 0005 superseded by 0011 & 0012)
- `learning/` — git-ignored personal notes: `code-walkthrough.md`, `how-to-run.md`,
  `session-log-phase1-spike.md`

## GitHub
Repo `itaim900/Fit-App`. Backlog = **4 milestones** (Phase 0–3) + **38 issues** + labels.

## How to run
See `docs/learning/how-to-run.md`. Short: `docker compose up -d` → build & run backend
(`cmake --preset default && cmake --build --preset default`, then `./build/fitapp_server.exe`) →
`npm run dev` in `frontend/` → open localhost:5173.

## Next steps
Phase 1 leftover: **GitHub Actions CI**. Then **Phase 2** (auth, profile, exercise catalog, manual
workouts) built test-first per `02-api-contract.md`.
