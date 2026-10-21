# WORKLOG — shared running log

Append-only session log shared across **both** interfaces (Claude Code in VS Code / Cursor, and
Cowork). Newest entry at the top. This is how work done in one place carries over to the other:
whatever I do in a session, I record here before finishing.

**How to use:** at the start of a session, read this + `docs/CONTEXT.md`. At the end, add an entry
below with today's date. Keep entries short (3–8 lines): what changed, why, decisions, what's next.
For durable/architectural facts, also update `docs/CONTEXT.md` (and add an ADR when warranted).

---

## 2026-10-21 — VS Code
- **Expanded `CLAUDE.md`** (via `/init`): kept the session rules and added build/test commands
  (including how to run a single GoogleTest or Vitest test), env vars, the server hexagonal layout
  (the two repository layers: plain Pg vs AutoCommit), auth as a plain function instead of Crow
  middleware, the testing pyramid, and the client layout. No code changes.
- **Step 0 re-verification after ~3 months away:** client 22/22, server 122/122 (0 skipped),
  live health OK via Vite proxy. Found: folder rename `Fit App`→`Kinetiq` breaks
  `docker compose up -d` (use `docker start kinetiq-postgres`); turned off Docker Desktop
  Kubernetes (3 stray `hello-web` pods eating RAM).
- **Git hooks** (brainstorm → spec → plan → inline TDD-style execution): tracked `.githooks/`
  (`lib.sh` + `pre-commit` + `pre-push`), `.gitattributes` forces LF. pre-commit checks only the
  changed half and warns on skipped DB tests; pre-push runs everything and blocks on skips (detected
  from GoogleTest's `[  SKIPPED ] N` line, because the binary exits 0 when DB tests skip). All 7
  spec scenarios verified (docs-only skip, failing test blocks, per-half selection, DB-down
  warn/block, healthy push).
- **Injury type (July plan Tasks 1–2):** migration 0003 + `injury_type` through every layer; done
  together so every commit stayed green under the new hook.
- **Redesign → injury conditions catalog (brainstorm → spec → plan → 6 TDD tasks):** injuries now
  affect several muscles. 17 ICD-11-anchored known conditions (locked region/type/muscles) or
  unknown injuries (user picks from 14 fixed regions + 28 muscles). Migrations 0004 (catalog) +
  0005 (injury link); `ICatalogRepository` + Pg adapters; `InjuryService` two shapes; 3 new GET
  endpoints. 171/171 tests, 0 skipped; live-verified with curl. No open injury→muscle database
  exists; secondary muscles are our judgement (flagged for physio review).
- Next: Stage C (#25) — React profile + injuries forms with the not-medical-advice notice.

## 2026-07-25 — VS Code (3)
- **Reviewed Stages A+B with the owner** (architecture walkthrough, JWT trade-offs/cookie
  discussion — kept `localStorage` + 24h JWT for now, deferred cookie/refresh-token hardening to a
  future slice) then **designed and planned an extension**: injuries can now optionally reference
  the `Muscles` catalog. Owner's ask specifically noted not all injuries are muscle injuries
  (bone/tendon/joint matter too) — design keys off a new `injury_type` enum + optional `muscle_id`
  valid only when `injury_type == "muscle"` (never required even then), single muscle per injury
  (no join table), and seeds 16 muscles now from `01-data-model.md`'s already-documented Phase 2
  list so Slice 3's full catalog seed stays compatible. Also adds a small read-only `GET /muscles`
  (+`/{id}`) endpoint, needed both to validate `muscle_id` cleanly (422, not a raw FK-violation 500)
  and to unblock Stage C's future muscle picker.
- 6 decisions locked (E1–E6) in
  `docs/superpowers/specs/2026-07-25-injury-muscle-link-design.md`; full 8-task bite-sized TDD plan
  in `docs/superpowers/plans/2026-07-25-injury-muscle-link.md` (migration → domain →
  `IMuscleRepository` stack → `InjuryService` validation/cascade → `MuscleController` →
  extend `InjuryDto`/`InjuryController` → wire `main.cpp`). Both committed; **no code executed
  yet** — owner chose inline execution (not subagent-driven) for learning visibility, to start next
  session at Task 1.
- Verified several Crow API details against the vendored header before writing plan code (avoids a
  wasted build cycle next session): `crow::json::wvalue::list()` doesn't exist in this Crow version
  (already worked around in Stage B via the `wvalue(vector<wvalue>)` ctor); confirmed `.i()`/`.b()`/
  `.t()==type::Null` all exist. Also confirmed `Migrator` sorts migration files lexically by
  filename, so `0003_injury_muscle_link.sql` slots in correctly after `0002_core_schema.sql`.
- **One real gotcha flagged in the plan itself:** `DbTest`'s `ensureTestDatabaseReady()` silently
  marks the whole suite unavailable (tests SKIP, not FAIL) if a migration throws — so Task 1
  includes an explicit `psql` verification step (columns + seed count) rather than trusting `ctest`
  output alone to catch a broken migration.
- Pushed `slice-2-profile` to origin (14 commits at that point; now 16 with the two plan/spec
  commits from this session).
- Next: execute the injury/muscle link plan **Task 1 onward, inline**, same red→green→commit
  rhythm as Stages A/B.

## 2026-07-25 — VS Code (2)
- **Completed Stage B — `#20` injuries CRUD** (`/users/me/injuries`), branch `slice-2-profile`,
  6 commits, red→green→commit each: B1 `domain::Injury` + `InjuryPolicy` (5 tests); B2
  `IInjuryRepository` + `InMemoryInjuryRepository` fake (5); B3 `PgInjuryRepository` +
  `AutoCommitPgInjuryRepository` (10 incl. real Postgres); B4 one cohesive `app::InjuryService`
  with a shared `requireOwned()` → **`InjuryNotFoundError` → 404** for another user's injury (6);
  B5 `InjuryDto` + `InjuryController` (collection GET/POST + item PATCH/DELETE by `<int>` id, 7
  HTTP tests incl. both cross-user 404s); B6 wired `main.cpp`.
- **Ownership isolation** is the core security property here: update/delete on an injury you don't
  own returns 404 (never 403 — don't even leak that it exists), enforced in the service and proven
  at the HTTP layer with two users.
- **Crow API gotcha, caught before coding:** verified the watch-items from the plan against the
  vendored `crow_all.h` first — `crow::json::wvalue::list()` doesn't exist in this version, so the
  injuries array is built via the `wvalue(std::vector<wvalue>)` constructor, which makes an **empty
  list serialize as `[]`** (not `null`). `.b()` bool accessor and `<int>`→`int64_t` route param
  confirmed present.
- **Verified:** `ctest` **122/122** (was 94) with Postgres up; **live-verified** via curl —
  create 201 → list → bad-severity 422 → PATCH active=false 200 → DELETE 204 → empty list `[]` →
  no-token 401. Test user + injuries cleaned up (FK cascade).
- Next: Stage C (`#25` frontend — profile + injuries forms, first React Hook Form + Zod). Then
  merge `slice-2-profile` → `main`.

## 2026-07-25 — VS Code
- **Started Phase 2 Slice 2 (Profile & Injuries).** Brainstormed the whole-slice design and locked
  8 decisions (`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`): expand
  `domain::User` (not a separate entity), load-modify-save for `PATCH`, no null-clearing in v1,
  **defer `preferences`** until its consumer (recommender) exists, one cohesive `InjuryService`,
  keep `/auth/me` and `/users/me` distinct, domain-level validation, RFC 9457 field errors.
  Wrote a bite-sized TDD plan for Stage A (`docs/superpowers/plans/2026-07-17-slice-2a-profile-backend.md`).
- **Completed Stage A — `#19` `GET`/`PATCH /users/me`**, on branch `slice-2-profile`, 5 commits,
  red→green→commit each: A1 grew `domain::User` + `ProfilePolicy` validator (12 tests); A2
  extended `PgUserRepository` to map/persist all profile columns (NULL↔`std::optional`) + fake
  round-trip; A3 `app::UpdateProfile` (load-modify-save, guard-validate before persist); A4
  `ProfileDto` + new `UserController` (GET/PATCH on one Crow multi-method route, RFC 9457 field
  errors); A5 wired `main.cpp`. **No migration** — Slice 0 already built every column.
- Key decision realized in code: biometrics are **optional/NULL-able** (never forced), but any
  provided value is **hard-rejected** server-side if out of range (weight (0,500], height (0,300],
  etc.) — the client's future Zod schema will surface the same limits as a friendly "please check"
  hint. Server enforces, client guides.
- **Verified, not just claimed:** `ctest` **94/94** (was 70) with Postgres up so DB integration
  tests ran; **live-verified** via curl against the real server + Postgres (register → GET metric
  default → PATCH → GET persisted → invalid 422 → no-token 401), test user cleaned up.
- Next: Stage B (`#20` injuries CRUD) — write its own plan then execute; then Stage C (`#25` FE
  forms, first RHF+Zod). Branch `slice-2-profile` stays open until the slice is done.

## 2026-07-16 — VS Code (2)
- **Finished `#24` — Slice 1 (Auth) is now fully complete** (`#16`, `#17`, `#18`, `#24` all done).
  Built the Client auth UI, mirroring the backend's TDD rhythm onto Vitest + Testing Library +
  MSW: `api.ts` (register/login/me calls), `lib/tokenStorage.ts`, `RegisterForm`, `LoginForm`,
  `AuthPage` (mode toggle + forgot-password placeholder), `App.tsx` (session restoration on load
  via `GET /auth/me`, logout). 22 new tests, all green.
- Two decisions worth remembering: **`localStorage`** for the token, not in-memory-only — the
  original `phase-2-plan.md` note predated the 24h JWT expiry decision, and in-memory storage
  would've quietly defeated the point of that expiry (token wouldn't survive a page refresh).
  **Forgot password** is a visual-only placeholder ("coming soon") since no backend exists yet —
  avoids a dead-end form without pretending the feature is real.
- Designed the login screen's background together: sketched 3 directions (energetic/athletic,
  calm/minimal, technical/schematic) with concrete palettes before building; picked "Momentum" —
  near-black ground, one coral accent, a soft radial glow that breathes slowly behind the card.
  Caught a real bug via Playwright's actionability checks (not visual inspection): `overflow:
  hidden` on the backdrop, added to contain the glow's blur, also clipped real content on short
  viewports, making a link genuinely unreachable. Fixed by scoping to `overflow-x: hidden`.
- **Live-verified via browser automation** against the real server and database, not just
  `npm test`: register → session persists across a full page reload → logout → wrong-password
  error → login → logout → forgot-password placeholder. Test user cleaned up afterward.
- Full narrative (2 more steps, 18 total) in `docs/phase-2-slice-1-auth.md`. Next: **Slice 2 —
  Profile & Injuries** (`#19`, `#20`, `#25`).

## 2026-07-16 — VS Code
- **Finished `#18` — Slice 1's entire backend is now done** (`#16`, `#17`, `#18` all complete;
  only `#24` Client auth remains). Built, TDD'd throughout, red-then-green on nearly every loop:
  `app/` services (`RegisterUser`, `LoginUser`, `ChangePassword`), extended `IUserRepository` with
  `findById`/`update` (needed for `ChangePassword`, touching "finished" `#16` files again — normal
  as ports grow), `http/dto/AuthDto.h` + `http/controllers/AuthController` (all 4 endpoints:
  register/login/me/change-password), and a new production-only adapter
  `AutoCommitPgUserRepository` (opens a fresh connection+transaction per call — discovered while
  wiring `main.cpp` that a single shared `PgUserRepository`/transaction isn't safe across a
  multithreaded server's concurrent requests). `main.cpp` fully wired (new `JWT_SECRET` env var,
  dev fallback with a startup warning if used) and **live-verified manually**: ran the real
  server against real Postgres, curl'd the full register → login → me → change-password flow,
  everything matched, cleaned up the test user afterward.
- Found two real things worth knowing about: a genuine **timing side-channel** in login (wrong
  password ~224ms vs. unknown email ~0ms, despite an identical error message) — deferred as a
  documented v1 limitation, not fixed; and confirmed catch-order matters in the HTTP handlers
  (`WeakPasswordError`/`DuplicateEmailError` both derive from `std::runtime_error`, so specific
  catches must precede the general one).
- `ctest` = **70/70** (was 54 at the start of today's #18 HTTP-layer work). Full narrative
  (16 steps, every decision with reasoning) in `docs/phase-2-slice-1-auth.md`.
- Next: `#24` — Client auth UI (register/login forms, token storage, authed fetch). Different
  stack (React/TS) from everything above.

## 2026-07-12 — VS Code (2)
- Finished `#17`: `IPasswordHasher`/`Argon2PasswordHasher` (real argon2id, fresh random salt per
  call via OpenSSL `RAND_bytes`), `ITokenService`/`JwtTokenService` (HS256, **24h expiry** —
  locked decision, since v1 has no refresh tokens this number *is* the session length), and the
  `authenticate()` function in `http/middleware/Authenticator.h` — deliberately a plain function,
  not Crow middleware, since `HttpClient`'s `handle_full()` doesn't run middleware. All 3 pieces
  TDD'd (red via failed compile, green on first implementation attempt each time).
- Promoted auth deps from `kinetiq_tests`-only onto `kinetiq_server`: restructured `CMakeLists.txt`
  (jwt-cpp `FetchContent` moved out from under `KINETIQ_BUILD_TESTS`), updated `Server/Dockerfile`
  (build stage already had the dev packages via CI; runtime stage needed `libargon2-1` + `libssl3`
  added — **not build-verified locally**, this project never builds Docker images on the 8GB dev
  machine, so CI is the real check).
- Hit one snag: first rebuild after the CMake change failed to *link* `kinetiq_server.exe`
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
  `phase-2-plan.md` §8), then promote those deps from `kinetiq_tests` onto `kinetiq_server` +
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
