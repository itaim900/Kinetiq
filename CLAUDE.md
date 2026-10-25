# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

It is auto-loaded in **every** interface (Claude Code in VS Code / Cursor, and Cowork) and is the
shared entry point that keeps work aligned no matter where the owner is typing.

## First thing, every session
1. **Read `docs/CONTEXT.md`**: the dense state-of-the-project brief (current phase/slice, locked
   decisions, troubleshooting, next steps). It is the source of truth for *where things stand*.
2. **Read `WORKLOG.md`** (repo root): the append-only log of recent sessions across VS Code and
   Cowork. This is how the two interfaces stay connected.

## End of every working session (both interfaces)
Append a dated entry to `WORKLOG.md` (newest at top): what changed, why, decisions made, what's
next. Keep it to 3–8 lines. If a decision is durable/architectural, also update `docs/CONTEXT.md`
(and add an ADR in `docs/adr/` when warranted).

## Owner & working style
Owner is a **beginner** at C++ and React building this as a **learning + portfolio** project.
Prefer clear explanations, small steps, and planning over fast code. Explain *why*, not just *what*.
Feature work follows a spec → plan → TDD-execute flow; specs and plans live in
`docs/superpowers/specs/` and `docs/superpowers/plans/`, and each slice has a living build log in
`docs/phase-2-slice-*.md`.

## Project one-liner
AI-powered personalized workout-plan web app (Kinetiq). **Server:** C++20 / Crow / libpqxx /
PostgreSQL 16. **Client:** React 18 + TypeScript + Vite. Hexagonal architecture.

## Commands

### Server (C++, run from `Server/`)
Local toolchain is **MSYS2 UCRT64 GCC + CMake + Ninja**. `C:\msys64\ucrt64\bin` must be on PATH.
```sh
docker compose up -d                       # (repo root) dev Postgres on 5432; needed for server + DB tests
cmake --preset default                     # configure into Server/build (Debug)
cmake --build --preset default -j 2        # builds kinetiq_server + kinetiq_tests (-j 2: see below)
ctest --test-dir build --output-on-failure # full suite
./build/kinetiq_tests.exe --gtest_filter='InjuryService*'   # one suite / test (GoogleTest filter)
ctest --test-dir build -R InjuryController                  # or filter via ctest regex
```
- When a build gates the next command, check the real exit code (`; echo exit=$?`), not a piped
  `tail`, which masks failures.
- **Limit parallel jobs (`-j 2`)** on this 8 GB machine: a wide rebuild with Ninja's default
  parallelism ran out of memory (`cc1plus.exe: out of memory`). The git hooks do this already
  (`KINETIQ_BUILD_JOBS`, default 2).
- **New `.cpp` files must be added by hand** to the explicit source lists in
  `Server/CMakeLists.txt` (`kinetiq_server` and/or `kinetiq_tests`). There is no glob.
- CMake options: `KINETIQ_FETCH_LIBPQXX` (ON in CI/Docker: builds libpqxx 7.9.2 from source
  because Ubuntu's package is broken; OFF locally), `KINETIQ_BUILD_TESTS` (OFF in the Docker image).
- DB integration tests use a separate `kinetiq_test` database (auto-created, migrations applied) and
  **self-skip** if Postgres is unreachable, so a "green" run without Docker up skips them silently.

### Client (from `Client/`)
```sh
npm install
npm test                        # vitest run (jsdom + Testing Library + MSW)
npx vitest run src/features/auth/LoginForm.test.tsx   # single file
npx vitest run -t "shows error"                       # by test name
npm run build                   # tsc --noEmit + vite build (this is the type-check)
```
There is no linter configured.

### Running the app
Launch via the wrapper from the repo root, **not** the raw exe or `npm run dev`:
```powershell
./scripts/run-dev.ps1 -Target server   # :8080 (applies migrations on startup; exits if DB down)
./scripts/run-dev.ps1 -Target client   # :5173, Vite proxies /api -> 127.0.0.1:8080
```
The wrapper binds the process tree to a Windows Job Object so dev servers can't be orphaned
holding ports. Health check: `GET /api/v1/health` → `{"status":"ok","db":"connected"}`.
Full containerized stack (CI-built GHCR images): `docker compose -f docker-compose.prod.yml up -d`
→ `localhost:3000`.

Server env vars: `DB_HOST/DB_PORT/DB_NAME/DB_USER/DB_PASSWORD` (default `kinetiq`/5432),
`TEST_DB_NAME`, `JWT_SECRET` (insecure dev fallback with a warning), `BIND_ADDR` (default
`127.0.0.1`; `0.0.0.0` in containers), `MIGRATIONS_DIR`.

### Git hooks
Enable once per clone: `git config core.hooksPath .githooks`. `pre-commit` builds/tests only the
half (`Server/`, `Client/`) with staged changes and *warns* if DB tests were skipped; `pre-push`
runs everything (incl. `npm run build`) and *blocks* on skipped DB tests, so start Postgres before
pushing. Bypass in an emergency with `--no-verify`. Logic lives in `.githooks/lib.sh`.

## Server architecture (hexagonal, `Server/src/`)
Dependencies point inward: `http/` → `app/` → `domain/` + `ports/`; `adapters/` implement `ports/`.
The core (`domain/`, `app/`, `ports/`) never includes Crow or pqxx. Each folder has a README
describing its role.

- `domain/`: entities (`User`, `Injury`) and pure validators (`PasswordPolicy`, `ProfilePolicy`,
  `InjuryPolicy`). Header-only.
- `ports/`: abstract interfaces (`IUserRepository`, `IInjuryRepository`, `IPasswordHasher`,
  `ITokenService`).
- `app/`: use cases / services (`RegisterUser`, `LoginUser`, `ChangePassword`, `UpdateProfile`,
  `InjuryService`), depending only on ports. Updates use load-modify-save.
- `adapters/db/`: two layers per repository:
  - `PgXRepository` takes an **existing `pqxx::work`** (used directly by tests so one rolled-back
    transaction spans the test).
  - `AutoCommitPgXRepository` wraps it, opening a fresh connection + transaction **per call**. This
    is what `main.cpp` wires in, because Crow routes capture deps for the server's lifetime and a
    transaction can't be shared across concurrent requests. Follow this pattern for new repos.
  - `Migrator`: startup runner applying `Server/migrations/NNNN_*.sql` not yet in
    `schema_migrations`, fails closed. Schema changes = a new numbered migration file; the full v1
    schema already exists in `0002_core_schema.sql` (see `docs/01-data-model.md`).
- `adapters/crypto/`: `Argon2PasswordHasher` (argon2id), `JwtTokenService` (HS256, 24h, no refresh).
- `http/`: `controllers/` expose free functions `registerXRoutes(app, ports...)` called from
  `main.cpp`; `dto/` does JSON ⇄ domain mapping; errors are RFC 9457 problem+json with field errors.
- **Auth is a plain function, not Crow middleware**: protected handlers call
  `http::authenticate(req, tokens)` (`http/middleware/Authenticator.h`) and return 401 on
  `nullopt`. Reason: the in-process test harness (`app.handle_full()`) does not run middleware.
  Don't introduce Crow middleware on routes.
- Ownership: accessing another user's resource returns **404**, not 403. Login failures are a
  generic 401 (no email enumeration).
- `PATCH /users/me` is **JSON Merge Patch** (`null` clears; ADR-0017); every changed profile field is
  recorded via `IProfileHistoryRepository` (`user_profile_history`, migration 0006).
- All routes are under `/api/v1`. The REST contract, including test cases, is
  `docs/02-api-contract.md`; it is contract-first, so update it with any endpoint change.

## Server testing pyramid (`Server/tests/`)
One GoogleTest binary, `kinetiq_tests`:
1. Domain unit tests (policies).
2. Service tests against **hand-written in-memory fakes** in `tests/support/`
   (`InMemoryUserRepository`, …). No gmock: add a fake when adding a port.
3. Adapter integration tests deriving from the `DbTest` fixture (`tests/support/TestDb.h`): each
   test runs in a transaction that is rolled back; begin with
   `if (!available()) GTEST_SKIP();`.
4. HTTP tests via `tests/support/HttpClient.h`: real Crow routing/handlers, no socket.

TDD rhythm: write the failing test from the cases in `02-api-contract.md`, implement against the
port/fake, go green, refactor.

## Client architecture (`Client/src/`)
- `api.ts`: typed fetch wrappers; request/response types mirror the server DTOs.
- `features/<area>/`: components with co-located `*.test.tsx` (e.g. `features/auth/`).
- `lib/tokenStorage.ts`: JWT persisted in `localStorage`; `App.tsx` restores the session on load.
- `mocks/handlers.ts` + `mocks/server.ts`: MSW handlers used by tests (wired in `test/setup.ts`).
  Add a handler for every new endpoint the UI calls. They're **stateful** (`resetMockDb()` after
  each test, `seedInjury`, `setMockProfile`).
- Routing: `react-router` v7 in `App.tsx`; `AppShell` (top bar + tabs) wraps `/injuries`, `/profile`.
- All authenticated calls go through `authFetch` in `api.ts` (Bearer token, `ApiError` with
  per-field errors; a 401 triggers the logout handler `App` registers).
- Forms: React Hook Form + Zod; rules and request converters live in pure `*Schema.ts` modules
  (`features/injuries/injurySchema.ts`, `features/profile/profileSchema.ts`).
- Themes: colour tokens in `theme.css`, switched by `data-theme` on `<html>` (`lib/theme.ts`; an
  inline script in `index.html` prevents a flash). Units: `lib/UnitsContext.tsx` (`useUnits()`,
  saved as the account's `unit_preference`); server stores **metric only**, `lib/units.ts` converts.

## Conventions to respect (see `docs/CONTEXT.md` for the reasoning)
- Source lives in `Server/` and `Client/` only; never recreate `backend/` or `frontend/` folders
  (those names survive only as Docker service / GHCR image names).
- 8 GB RAM machine: don't run a heavy C++ build and Docker builds at the same time. Container
  images are built in CI (`.github/workflows/images.yml`), never locally.
- Windows socket libs are linked only under `if(WIN32)`; keep CMake changes Linux-safe, since CI
  builds on Ubuntu (`.github/workflows/ci.yml`).
- MSYS2 packages: always `pacman -Syu`, never a partial `pacman -Sy`.
- Test-first (TDD) for Phase 2 domain work.
