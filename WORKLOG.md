# WORKLOG — shared running log

Append-only session log shared across **both** interfaces (Claude Code in VS Code / Cursor, and
Cowork). Newest entry at the top. This is how work done in one place carries over to the other:
whatever I do in a session, I record here before finishing.

**How to use:** at the start of a session, read this + `docs/CONTEXT.md`. At the end, add an entry
below with today's date. Keep entries short (3–8 lines): what changed, why, decisions, what's next.
For durable/architectural facts, also update `docs/CONTEXT.md` (and add an ADR when warranted).

---

## 2026-07-12 — VS Code (2)
- Finished `#17`: `IPasswordHasher`/`Argon2PasswordHasher` (real argon2id, fresh random salt per
  call via OpenSSL `RAND_bytes`), `ITokenService`/`JwtTokenService` (HS256, **24h expiry** —
  locked decision, since v1 has no refresh tokens this number *is* the session length), and the
  `authenticate()` function in `http/middleware/Authenticator.h` — deliberately a plain function,
  not Crow middleware, since `HttpClient`'s `handle_full()` doesn't run middleware. All 3 pieces
  TDD'd (red via failed compile, green on first implementation attempt each time).
- Promoted auth deps from `fitapp_tests`-only onto `fitapp_server`: restructured `CMakeLists.txt`
  (jwt-cpp `FetchContent` moved out from under `FITAPP_BUILD_TESTS`), updated `Server/Dockerfile`
  (build stage already had the dev packages via CI; runtime stage needed `libargon2-1` + `libssl3`
  added — **not build-verified locally**, this project never builds Docker images on the 8GB dev
  machine, so CI is the real check).
- Hit one snag: first rebuild after the CMake change failed to *link* `fitapp_server.exe`
  ("Permission denied") — a stale server process from earlier in the session (launched via
  `run-dev.ps1`, never stopped) still had the file open. Killed it, rebuilt clean.
- `ctest` = **39/39** (was 27 at the start of this segment). **`#16` and `#17` are both complete.**
- Next: `#18` — HTTP-level tests + handlers for register/login/me/change-password, likely via
  small `app/` services wiring today's pieces together for the first time.

## 2026-07-12 — VS Code
- Started Phase 2 Slice 1 (Auth), TDD throughout (red confirmed via failed compile/assertions,
  then green each time): `domain::User`, `domain::PasswordPolicy` (settled on min 8 chars + one
  uppercase letter after a same-day revision), `ports::IUserRepository` + `DuplicateEmailError`,
  the `InMemoryUserRepository` fake, and `db::PgUserRepository` (real Postgres adapter, same
  contract as the fake). **`#16` (repository ports + fakes) is now fully complete.**
- Locked scope decisions, each recorded with reasoning in `02-api-contract.md`: generic `401` on
  login failure (no email enumeration), added `PATCH /auth/password` (change password while
  logged in) to Slice 1; explicitly **deferred** Google OAuth, "forgot password" reset, account
  deletion, login rate-limiting, and email verification to future slices.
- Added `docs/phase-2-slice-1-auth.md`: a living, step-by-step build log for the slice (decisions,
  TDD loops, files touched, progress checklist) — keep updating it as the slice continues, don't
  wait until it's done.
- `ctest` = **27/27** (was 13 at session start).
- Next: `#17` — argon2 password hasher + JWT issue/verify + authenticator function (deliberately
  **not** Crow middleware, since the in-process `HttpClient` test harness doesn't run it — see
  `phase-2-plan.md` §8), then promote those deps from `fitapp_tests` onto `fitapp_server` +
  `Server/Dockerfile`.

## 2026-07-09 — VS Code
- Onboarded (fresh interface): re-read `docs/CONTEXT.md` + this log, confirmed Phase 1 + Phase 2
  Slice 0 status. Ran the full dev stack (Postgres, server, client) to verify the machine still
  works end-to-end — all green, 13/13 `ctest` passing.
- Did a guided code walkthrough of the `/api/v1/health` request path (Crow route in `main.cpp` →
  DB check → JSON response → Vite proxy → React `App.tsx`) for the owner's learning goals; noted
  the hexagonal folders (`http/ app/ domain/ ports/`) are still empty scaffolding until Slice 1.
- Found + fixed a real bug: background dev-server processes from past sessions (7/5, 7/6) were
  never terminated, left running as orphans, and squatted ports 5173/5174 — causing Vite to drift
  to 5175 in this session and confusing "is the port stable" diagnosis.
- Added `scripts/run-dev.ps1`: wraps server/client launch in a Windows Job Object
  (`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) so the whole child process tree dies automatically if the
  wrapper process ends for any reason (Ctrl-C, window close, forced kill) — verified through 4
  levels of nesting for the client (npm → cmd → vite → node). Documented in
  `docs/learning/how-to-run.md` (git-ignored, not in this commit).
- Next: use `./scripts/run-dev.ps1 -Target server|client` going forward instead of raw
  exe/`npm run dev`. Continue Phase 2, Slice 1 (Auth) — see `docs/phase-2-plan.md`.

## 2026-07-08 — Cowork
- Set up cross-interface continuity: added root `CLAUDE.md` (auto-loaded by both VS Code and Cowork)
  and this `WORKLOG.md`. Both now point at `docs/CONTEXT.md` as the source of truth.
- Why: owner wants to work in VS Code while staying aligned here. Chat histories don't sync between
  interfaces, but shared files in the repo do — so context lives in files, not the transcript.
- Next: use this log going forward. Continue Phase 2, Slice 1 (Auth) — see `docs/phase-2-plan.md`.

<!-- Template for new entries (copy above this line):

## YYYY-MM-DD — <VS Code | Cowork>
- What changed:
- Why / decisions:
- Next:

-->
