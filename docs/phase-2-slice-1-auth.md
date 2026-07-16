# Phase 2, Slice 1 — Auth: Step-by-Step Build Log

**Status:** 🟡 IN PROGRESS (started 2026-07-12) — `#16`, `#17`, `#18` complete; `#24` (Client) remains
**Issues:** #16, #17, #18, #24 (see `phase-2-plan.md` §5, §9)
**Contract:** `02-api-contract.md` (Auth section) — the source of truth for exact request/response
shapes and test cases; this doc is the narrative of *how* and *in what order* we got there.
**Goal:** users can register, log in, view their own basic profile, and change their password —
every other Phase 2 endpoint depends on this (🔒). This is a **living document** — update it as
the slice progresses, don't wait until it's done.

---

## Progress checklist

- [x] Decisions locked (password policy, login error shape, scope: what's in/out) — 2026-07-12
- [x] `domain::User` — 2026-07-12
- [x] `domain::PasswordPolicy` — 2026-07-12 (revised same day)
- [x] `ports::IUserRepository` + `DuplicateEmailError` — 2026-07-12
- [x] `tests/support::InMemoryUserRepository` (fake) — 2026-07-12
- [x] `db::PgUserRepository` (real Postgres adapter) — 2026-07-12. **#16 complete.**
- [x] `IPasswordHasher` port + `Argon2PasswordHasher` adapter — 2026-07-12
- [x] `ITokenService` port + `JwtTokenService` adapter, 24h expiry — 2026-07-12
- [x] Authenticator function (`Authorization: Bearer` → `CurrentUser` or `nullopt`) — 2026-07-12
- [x] Promoted auth deps from `fitapp_tests`-only to `fitapp_server` + `Server/Dockerfile` —
  2026-07-12. **#17 complete.**
- [x] `app::RegisterUser` service (happy path, weak password, duplicate email) — 2026-07-16.
- [x] `app::LoginUser` service (happy path, wrong password, unknown email) — 2026-07-16.
- [x] `IUserRepository` extended with `findById` + `update` (fake + `PgUserRepository`) — 2026-07-16.
- [x] `app::ChangePassword` service (happy path, wrong current password, weak new password) —
  2026-07-16.
- [x] `POST /auth/register` (DTOs + handler + HTTP tests) — 2026-07-16
- [x] `POST /auth/login` (handler + HTTP tests) — 2026-07-16
- [x] `GET /auth/me` (handler + HTTP tests) — 2026-07-16
- [x] `PATCH /auth/password` (handler + HTTP tests) — 2026-07-16
- [x] `AutoCommitPgUserRepository` (production repository wrapper) — 2026-07-16
- [x] Wired into `main.cpp` + live-verified against the real server/DB — 2026-07-16. **#18
  complete.**
- [ ] Client: register/login forms, token storage, authed fetch — #24

---

## Decisions (chronological)

### 2026-07-12 — Login error shape: generic, no email enumeration
Kept the existing contract: wrong password and unknown email both return `401` with an
**identical** message. Rejected the alternative (revealing "no account found") because it would
let an attacker probe which emails are registered. See `02-api-contract.md` Auth § login.

### 2026-07-12 — Password policy: decided, then revised same day
First pass: length-only, min 10 chars (NIST 800-63B style — length over composition rules).
**Revised later the same session** to **min 8 chars + at least one uppercase letter**, per owner
preference. Superseding an already-locked decision within the same day is fine — nothing had
shipped against it yet, and the test suite caught the change cleanly (see "TDD loop 1" below).

### 2026-07-12 — Google OAuth: deferred to its own future slice
Not in scope for #16–#18. Needs a Google Cloud OAuth client, server-side ID token verification
against Google's JWKS, and a schema change (nullable `password_hash` + an auth-provider marker on
`Users`) — a slice of its own, not a test case bolted onto this one.

### 2026-07-12 — "Forgot password" reset flow: deferred
Same shape of reason as OAuth: needs an email-sending dependency (none exists in the stack yet), a
time-limited single-use reset-token table, and its own security design (token expiry,
rate-limiting the request endpoint, and the same no-enumeration question login already answered,
re-litigated for "does this email exist" in the reset-request step). **Change password while
logged in** (below) covers the common case without needing any of that.

### 2026-07-12 — Account deletion: deferred
Considered as part of "full CRUD on the user." Not just a missing route — a destructive action
needs its own design (require re-authentication before deleting? soft- vs hard-delete, given
`Users` is referenced by `Workouts`, `User_Injuries`, `Generation_Requests` via FK). Left out of
Slice 1; `Update` (`PATCH /users/me`) was already Slice 2 scope regardless.

### 2026-07-12 — JWT access token lifetime: 24 hours
v1 has no refresh tokens (locked decision), so this number *is* the session length. Rejected
1 hour (would force re-login roughly hourly during real use, likely to feel broken) and 7 days
(too large a window if a token leaks). 24 hours balances "log in once a day" convenience against
keeping a leaked-token blast radius same-day. Implemented as a constructor parameter on
`JwtTokenService` (not hardcoded), so tests can construct one with a short/negative TTL to
exercise expiry synchronously.

### 2026-07-12 — Change password (while logged in): added to Slice 1
No email infra needed — the user is already authenticated, submits current + new password, the
service re-verifies and re-hashes. New endpoint: `PATCH /auth/password` 🔒. Full spec + test
cases in `02-api-contract.md`.

### 2026-07-16 — Validation order in `app/` services: cheap/local checks before expensive/side-effecting ones
`RegisterUser::execute()` calls `requireStrongPassword()` as its very first line, before hashing
or touching the repository. This is a **guard clause**: validate preconditions immediately and
bail out before anything downstream runs, rather than validating after the fact and having to
undo work. Concretely, it means a rejected weak-password request costs ~0ms instead of paying
argon2's deliberate ~200ms–1.5s hashing cost for input we're about to throw away — proven by the
test itself running in 0ms vs. the happy path's 271ms. Generalizes forward: the same reasoning
means the *duplicate-email* check (next TDD loop) should probably also happen before hashing, not
after — currently `save()` only discovers the duplicate at the very end, after a strong password
has already paid the full hashing cost. Decide explicitly when building that step rather than
leaving the order accidental. (Resolved 2026-07-16: kept as-is, letting `DuplicateEmailError`
propagate from `save()` unchanged — see Step 10 below.)

### 2026-07-16 — Login timing side-channel: accepted as a known v1 limitation
`LoginUser`'s wrong-password and unknown-email cases return an identical error *message* (per the
earlier no-enumeration decision), but not identical *timing* — the unknown-email path
short-circuits before ever calling the hasher, so it responds in ~0ms vs. wrong-password's
~200ms+ (argon2's real cost). This was discovered empirically, not just reasoned about: the actual
test run showed `WrongPassword_ThrowsInvalidCredentials` at 224ms and
`UnknownEmail_ThrowsInvalidCredentials` at 0ms. The standard fix (always call `verify()` against a
dummy hash, generated via the real hasher rather than hand-crafted, so both paths cost the same)
was considered and explicitly **deferred** — this app isn't a high-value target, and exploiting a
timing channel needs many precise measurements for a low-value payoff here. Documented in
`02-api-contract.md` under `POST /auth/login` as a deliberate gap, not an oversight; revisit if
the threat model ever changes.

### 2026-07-16 — Fake password hasher: still deferred
After `ChangePassword`, the full suite reached 54/54 in ~22s (up from single digits earlier in the
slice), entirely from real argon2 calls compounding across `RegisterUser`/`LoginUser`/
`ChangePassword` tests plus their seed-user setup. Raised the fake-hasher question flagged back in
Step 6. **Kept the real hasher for now** — 22s isn't painful yet, and the upcoming HTTP layer is a
smaller, more bounded set of tests (4 endpoints) than the open-ended service-layer work just
finished. Revisit if HTTP-layer tests push this meaningfully higher.

### 2026-07-16 — Production repository needs its own class: `AutoCommitPgUserRepository`
Discovered while wiring `main.cpp`, not anticipated earlier: `registerAuthRoutes(app, users,
hasher, tokens)` is called *once* at server startup and captures its dependencies by reference for
the server's entire lifetime (every test already relies on this). But `PgUserRepository` needs a
`pqxx::work&` — a transaction — which is inherently per-operation and unsafe to share across the
concurrent requests a multithreaded Crow server handles. `Argon2PasswordHasher` and
`JwtTokenService` have no such problem (no per-request state), so only the repository needed a
fix. Added `AutoCommitPgUserRepository`: opens a fresh connection + transaction for every single
call, delegates to `PgUserRepository`, commits — "auto-commit" in the classic DB sense. This
mirrors a pattern already in `main.cpp`'s health-check handler (opens its own fresh
`pqxx::connection` per request rather than sharing one), so it's consistent with, not a departure
from, this codebase's existing convention. **Known, accepted limitation:** multi-step use cases
(e.g. `ChangePassword`'s `findById()` then `update()`) do not get one atomic transaction across
both calls. The only way to observe this is a narrow, low-severity race (the same user firing two
concurrent requests); properly closing it would mean threading a transaction through Crow's
request lifecycle, which nothing in this codebase does yet. Not worth the complexity for v1.

---

## Step-by-step build log

### Step 1 — `domain::User` (2026-07-12)
`Server/src/domain/User.h`. Plain struct (`id`, `email`, `passwordHash`, `displayName`) — the
subset of the `Users` table auth needs; full profile fields arrive in Slice 2. No TDD loop here:
it's a data type, nothing to assert against.

### Step 2 — `domain::PasswordPolicy` (2026-07-12, TDD loop 1)
1. Wrote `Server/tests/test_password_policy.cpp` against a `domain/PasswordPolicy.h` that didn't
   exist yet.
2. Built — **red**: `fatal error: domain/PasswordPolicy.h: No such file or directory`.
3. Wrote `Server/src/domain/PasswordPolicy.h` (`isPasswordStrong`), initially length-only ≥10.
4. Built + ran — **green**, 3/3.
5. **Later the same session:** owner revised the rule to min 8 + uppercase. Rewrote the test
   cases first (`RejectsPasswordsWithNoUppercaseLetter`, etc.) → built → **red** (2 failures,
   exactly as expected: `alllowercase` wrongly accepted, `Abcdefgh` wrongly rejected under the old
   rule) → rewrote the implementation → built → **green**, 4/4.

### Step 3 — `ports::IUserRepository` + `InMemoryUserRepository` (2026-07-12, TDD loop 2)
1. Wrote `Server/tests/test_user_repository.cpp` against `ports/IUserRepository.h` and
   `support/InMemoryUserRepository.h`, neither of which existed yet.
2. Built — **red**: `fatal error: domain/User.h: No such file or directory`.
3. Wrote `Server/src/ports/IUserRepository.h` (`save`, `findByEmail`, `DuplicateEmailError`) and
   `Server/tests/support/InMemoryUserRepository.h` (fake, keyed on lowercased email to mirror
   Postgres's `citext` case-insensitivity — so tests written against the fake hold for the real
   `PgUserRepository` too).
4. Built + ran — **green**, 5/5. Full suite at this point: **21/21** (13 Slice-0 baseline + 8 new).

### Step 4 — Docs updated to match (2026-07-12)
`02-api-contract.md` (password policy + test cases, deferred-items list, new `PATCH
/auth/password` spec), `phase-2-plan.md` §5/§9 (Slice 1 bullet, scope), `CONTEXT.md` (status,
docs map), this file created.

### Step 5 — `db::PgUserRepository` (2026-07-12, TDD loop 3 — different pyramid layer)
The first **adapter integration** test in this slice (vs. domain/unit so far) — needs a real,
running Postgres, unlike everything before it.
1. Wrote `Server/tests/test_pg_user_repository.cpp`: the same 5 cases as
   `test_user_repository.cpp`, but as `TEST_F(DbTest, ...)` against a `PgUserRepository` that
   didn't exist yet.
2. Built — **red**: `fatal error: adapters/db/PgUserRepository.h: No such file or directory`.
3. Wrote `Server/src/adapters/db/PgUserRepository.h/.cpp`: real SQL (`INSERT ... RETURNING id`,
   `SELECT ... WHERE email = $1`), taking a `pqxx::work&` it doesn't own (so it composes with
   `DbTest`'s per-test rollback transaction rather than conflicting with it — a `pqxx::connection`
   can only have one transaction open at a time), and catching `pqxx::unique_violation` to
   re-throw as `ports::DuplicateEmailError`.
4. Built — clean compile, but with `exec_params` deprecation warnings (this libpqxx version
   prefers `exec(zview, params)`). Fixed before even running the tests — no reason to carry a
   warning forward.
5. Ran the 5 new tests — **green on the first attempt**, no assertion-failure iteration needed.
6. Full suite: **27/27** (22 previous + 5 new). **`#16` is now complete.**

### Step 6 — `IPasswordHasher` + `Argon2PasswordHasher` (2026-07-12, TDD loop 4)
1. Wrote `Server/tests/test_password_hasher.cpp` (round-trip, wrong password fails, and — a real
   security property, not just a nice-to-have — two hashes of the *same* password must differ,
   proving a fresh random salt is used each call) against a class that didn't exist.
2. Built — **red**: `fatal error: adapters/crypto/Argon2PasswordHasher.h: No such file or
   directory`.
3. Verified the exact argon2 C API in the installed header (`argon2.h`) before writing anything —
   `argon2_encodedlen`, `argon2id_hash_encoded`, `argon2id_verify`, `argon2_error_message` —
   rather than guessing signatures for a security-relevant function.
4. Wrote `Server/src/ports/IPasswordHasher.h` and
   `Server/src/adapters/crypto/Argon2PasswordHasher.{h,cpp}`: generates a fresh random salt per
   call via OpenSSL's `RAND_bytes` (argon2's encoded output embeds the salt, so `verify()` doesn't
   need it separately).
5. Built + ran — **green**, 3/3 (slow: ~1.3–2.4s per test, argon2's deliberate memory-hardness —
   64 MiB cost, same params the Slice 0 smoke test already proved work on this toolchain). This is
   exactly why a fast in-memory fake will matter once #18 needs many service-level tests.

### Step 7 — `ITokenService` + `JwtTokenService` (2026-07-12, TDD loop 5)
1. Wrote `Server/tests/test_token_service.cpp`: issue→verify round-trip, wrong secret rejected,
   malformed token rejected, and expired token rejected (constructed with a **negative** TTL so
   the token is already expired the instant it's issued — lets the test assert synchronously
   instead of sleeping).
2. Confirmed jwt-cpp's `exp` claim is validated **automatically** by `verifier.verify()` whenever
   present (checked the vendored header directly rather than assuming) — so `set_expires_at()` at
   issue time is all that's needed; no separate expiry-check call.
3. Built — **red**: `fatal error: ... JwtTokenService.h: No such file or directory`.
4. Wrote `Server/src/ports/ITokenService.h` and `Server/src/adapters/crypto/JwtTokenService.{h,cpp}`
   — HS256, issuer `"fitapp"`, subject = user id, TTL as a constructor parameter (not hardcoded)
   so tests can control it.
5. Built + ran — **green**, 4/4 on the first attempt.

### Step 8 — Authenticator (2026-07-12, TDD loop 6)
1. Wrote `Server/tests/test_authenticator.cpp` directly constructing `crow::request` objects
   (no HTTP harness needed) covering all 5 cases from `phase-2-plan.md` §8's design note: valid,
   missing header, wrong scheme (`Basic` instead of `Bearer`), malformed token, expired token.
2. Built — **red**: `fatal error: http/middleware/Authenticator.h: No such file or directory`.
3. Wrote `Server/src/http/middleware/Authenticator.h` — a plain function (`authenticate(req,
   tokens) -> optional<CurrentUser>`), deliberately **not** Crow middleware, since `HttpClient`'s
   `app.handle_full()` doesn't run middleware at all. Removed the now-redundant `.gitkeep` from
   that folder.
4. Built + ran — **green**, 5/5 on the first attempt.

### Step 9 — Promote auth deps to `fitapp_server` (2026-07-12)
Restructured `CMakeLists.txt`: moved the jwt-cpp `FetchContent` block out from under
`FITAPP_BUILD_TESTS` (so it's available unconditionally), added `Argon2PasswordHasher.cpp` +
`JwtTokenService.cpp` to `fitapp_server`'s sources, and called `fitapp_add_auth_deps(fitapp_server)`.
Hit one snag: the very first rebuild failed to *link* (not compile) with "Permission denied" — a
stale `fitapp_server.exe` from earlier in the session (started via `run-dev.ps1`, never stopped)
still had the file open. Killed it, rebuilt clean.

Updated `Server/Dockerfile`'s runtime stage to install `libargon2-1` + `libssl3` (the build stage
already needed `libargon2-dev` + `libssl-dev`, which CI's Ubuntu job already had from Slice 0).
**Not verified by a local build** — this project never builds Docker images locally (8GB RAM
constraint) — CI is the real check, same situation as the earlier libpqxx/`FITAPP_FETCH_LIBPQXX`
packaging issue.

Full suite after all of #17: **39/39** (27 previous + 12 new). **`#17` is now complete.**

### Step 10 — `app::RegisterUser` (2026-07-16, TDD loops 7 & 8)
The first `app/` service — orchestrates everything Slice 1 has built so far. Done as two small,
separately-committed-to-history TDD loops rather than one big one, per owner preference to go
slowly through #18.

**Loop 7 — happy path:**
1. Wrote `Server/tests/test_register_user.cpp` with one case (valid request → user created,
   token issued, verifiable) against `app/RegisterUser.h`, which didn't exist yet. Test injects
   real `Argon2PasswordHasher` + `JwtTokenService` + fake `InMemoryUserRepository` through their
   *port* types — deliberately no fakes for hasher/tokens yet, since with only one test the ~230ms
   argon2 cost isn't worth avoiding. Revisit once test count makes it hurt.
2. Built — **red**: `fatal error: app/RegisterUser.h: No such file or directory`.
3. Wrote `Server/src/app/RegisterUser.h`: constructor takes `IUserRepository&`/`IPasswordHasher&`/
   `ITokenService&` (ports, never concrete adapters, per `app/README.md`). `execute()` hashes,
   saves, issues a token, returns both. Doesn't catch anything itself — errors (e.g.
   `DuplicateEmailError`) propagate straight through; translating exceptions into HTTP status codes
   is the controller's job, not the service's.
4. Built + ran — **green**, first attempt. Asserted a real security property along the way:
   `result.user.passwordHash != "StrongPass1"` (the plaintext never gets stored as-is).

**Loop 8 — weak password rejected:**
1. Added a test asserting a 5-char password throws `domain::WeakPasswordError` *and* that nothing
   was persisted (`users.findByEmail(...)` still empty afterward).
2. Built — **red**: compile error, `WeakPasswordError` didn't exist as a type yet.
3. Added `WeakPasswordError` + a `requireStrongPassword()` helper to `domain/PasswordPolicy.h`
   (not to `RegisterUser.h`) — deliberately placed next to the rule it enforces rather than next
   to today's only caller, since `ChangePassword` will need to enforce the identical policy on its
   `new_password` field soon. Called `requireStrongPassword()` as the very first line of
   `execute()`, before hashing or touching the repository (a **guard clause** — see the 2026-07-16
   decision above for why the ordering itself matters, not just that the check exists).
4. Built + ran — **green**, first attempt. The weak-password test ran in **0ms** vs. the happy
   path's 271ms — concrete proof it short-circuits before ever reaching the hasher.

**Loop 9 — duplicate email:**
1. Owner decision: keep the existing order (validate → hash → save) rather than moving the
   duplicate-email check before hashing, even though the same "avoid wasted work" reasoning from
   Loop 8 would apply.
2. Added a test: register `carol@example.com`, then register it again with a different password
   → expect `ports::DuplicateEmailError`.
3. Built — **compiled and passed immediately, no production code changed.** `execute()` already
   has no `try`/`catch` around `users_.save(user)`, so the repository's `DuplicateEmailError` was
   already propagating untouched — this test *documents and regression-guards* existing behavior
   rather than driving new code. Still a legitimate, valuable test: it fails if a future change
   accidentally wraps `execute()` in a catch block that swallows the error.

Full suite after `RegisterUser`: **42/42** (39 previous + 3 new).

### Step 11 — `app::LoginUser` (2026-07-16, TDD loop 10)
Unlike `RegisterUser`'s two separable checks, "find the user" and "check the password" are one
cohesive operation here, so happy path + the failure case were built together as a single loop.

1. Wrote `Server/tests/test_login_user.cpp`: correct credentials → user + token; wrong password →
   throws; unknown email → throws — the **same exception type** (`InvalidCredentialsError`) for
   both failure cases, deliberately, so nothing downstream can accidentally distinguish them (the
   type system enforces the no-enumeration principle, not just careful coding).
2. Built — **red**: `fatal error: app/LoginUser.h: No such file or directory`.
3. Wrote `Server/src/app/LoginUser.h`: `if (!found.has_value() || !hasher_.verify(...))` — a
   single branch, single throw, for both failure modes.
4. Built + ran — **green**, first attempt (3/3). Full suite: **45/45**.
5. **Discovered a real timing side-channel from the test's own output**, not from reasoning about
   it in the abstract: `WrongPassword_ThrowsInvalidCredentials` took 224ms;
   `UnknownEmail_ThrowsInvalidCredentials` took 0ms. Short-circuit evaluation means an unknown
   email never reaches `hasher_.verify()`, so despite an identical error *message*, response
   *timing* still reveals which failure occurred. Raised it, owner chose to **defer** rather than
   add a dummy-hash `verify()` call — documented as a deliberate v1 limitation in
   `02-api-contract.md` and the decision log above, not a bug.

### Step 12 — Extend `IUserRepository`: `findById` + `update` (2026-07-16, TDD loop 11)
`ChangePassword` needs to look up the authenticated user by id (from the JWT subject, not their
email) and persist a new password hash for an *existing* user -- neither operation existed on the
port yet (`save()` is create-only). Touches "finished" `#16` files again, which is normal: ports
grow as new use cases demand it.

1. Added 3 cases each to `test_user_repository.cpp` (fake) and `test_pg_user_repository.cpp`
   (real, `DbTest`-based): `findById` returns the user, `findById` on an unknown id returns
   `nullopt`, `update` changes the stored fields (and `findByEmail` reflects the change too, not
   just `findById` -- proving the fake's dual-lookup stays consistent after a mutation).
2. Built — **red**: `'class fitapp::db::PgUserRepository' has no member named 'findById'`.
3. Added `findById`/`update` to `ports/IUserRepository.h`. Implemented in
   `InMemoryUserRepository` (linear scan by id -- fine for a fake's tiny datasets; `update()`
   re-keys the map if the email changed, matching what a real `UPDATE` would do even though no
   current caller changes email) and `PgUserRepository` (`SELECT ... WHERE id = $1`,
   `UPDATE ... WHERE id = $4`, same `pqxx::unique_violation` → `DuplicateEmailError` translation
   as `save()`). Factored the row→`User` mapping into a local `rowToUser()` helper to avoid
   duplicating it between `findByEmail` and the new `findById`.
4. Built + ran — **green**, 16/16 (8 fake + 8 real) on the first attempt.

### Step 13 — `app::ChangePassword` (2026-07-16, TDD loop 12)
1. Wrote `Server/tests/test_change_password.cpp`: correct current password updates the hash (new
   password verifies, old one no longer does); wrong current password throws
   `InvalidCredentialsError` (reused from `LoginUser.h` rather than a new near-duplicate type --
   same meaning, same principle) and leaves the password unchanged; weak new password throws
   `WeakPasswordError` and also leaves the password unchanged.
2. Built — **red**: `fatal error: app/ChangePassword.h: No such file or directory`.
3. Wrote `Server/src/app/ChangePassword.h`: same guard-clause ordering as `RegisterUser` --
   `requireStrongPassword(newPassword)` runs before `findById` or `hasher_.verify()`. Combines
   "user not found" (defensive; shouldn't happen for an authenticated request, but handled the
   same way regardless) and "wrong current password" into the identical `InvalidCredentialsError`.
4. Built + ran — **green**, 3/3 on the first attempt. Full suite: **54/54**.

All three `app/` services for #18 are now done (`RegisterUser`, `LoginUser`, `ChangePassword`).

### Step 14 — First DTOs + `POST /auth/register` HTTP layer (2026-07-16, TDD loop 13)
The first `http/dto/` and `http/controllers/` files in the codebase. Also the first place actual
Crow JSON-parsing API gets used (only response-*building* existed before, in `main.cpp`'s health
check) — verified `crow::json::load`'s exact `rvalue` API (`operator[]`, `.s()`, `.has()`,
`explicit operator bool()`) directly in the vendored header before writing anything, same
discipline as verifying the argon2 C API back in Step 6.

1. Wrote `Server/tests/test_auth_controller.cpp`, one happy-path case, driving
   `fitapp::http::registerAuthRoutes(app, users, hasher, tokens)` through `HttpClient` — a function
   that didn't exist yet.
2. Built — **red** (CMake-level, like Step 5: `add_executable` couldn't find
   `AuthController.cpp`, which didn't exist at all yet).
3. Wrote `Server/src/http/dto/AuthDto.h` (`parseRegisterRequest`, `userToJson` — deliberately
   excludes `passwordHash`, `authResultToJson`) and
   `Server/src/http/controllers/AuthController.{h,cpp}` (`registerAuthRoutes`, a free function
   matching `Authenticator.h`'s pattern, not a class). Confirmed `crow::response(int, returnable&)`
   exists in the vendored header before relying on `crow::response(201, json)`. Removed the
   now-redundant `.gitkeep` files from both new folders.
4. Built + ran — **green**, first attempt. First genuine end-to-end proof: HTTP request → Crow
   routing → JSON parsing → `RegisterUser` → hasher/repo/tokens → JSON response, `password_hash`
   confirmed absent from the wire.
5. Added the 3 failure cases (weak password → 422, duplicate email → 409, missing field → 422).
   Built — **red**: all three returned `500` (uncaught exceptions), exactly as expected. Added
   `dto::errorResponse()` (a small RFC 9457-shaped helper) and a `try`/`catch` in the handler.
   **Catch-order matters**: `WeakPasswordError` and `DuplicateEmailError` both derive from
   `std::runtime_error`, so they must be caught *before* a general `catch (const std::exception&)`
   fallback (which handles missing/malformed JSON fields) — reversing the order would let the
   general catch swallow the specific ones first. Built + ran — **green**, 4/4.

### Step 15 — `POST /auth/login`, `GET /auth/me`, `PATCH /auth/password` (2026-07-16, TDD loops 14-16)
Same rhythm repeated three times, each red-then-green on the first implementation attempt:
- **Login**: `parseLoginRequest` + a route catching `InvalidCredentialsError` → `401` (same message
  for wrong password and unknown email, per the earlier no-enumeration decision). 3 tests.
- **Me**: the first route to use `Authenticator::authenticate()` from `#17` for real. Missing/
  invalid/expired token → `401` via the authenticator; a valid-but-now-nonexistent user (edge case,
  defensive) also → `401`. Response is the bare user profile, not wrapped in a `user` key (unlike
  register/login, which also return a token). 3 tests.
- **Change password**: needs *both* the authenticator (for the user id) *and* `ChangePassword`.
  Checks the token before parsing the body. 4 tests, including a full round-trip through `login`
  itself (old password stops working, new one works) as part of the same test.

Full suite after all HTTP routes: **68/68** (14 `AuthControllerTest` cases total).

### Step 16 — Wire into `main.cpp` (2026-07-16)
The composition root, for real. Needed `AutoCommitPgUserRepository` first (see the 2026-07-16
decision above) since naively constructing one `PgUserRepository` at startup would have been
unsafe. Added `JWT_SECRET` env var (dev fallback, but — unlike `DB_PASSWORD`'s silent fallback —
logs a `CROW_LOG_WARNING` when the fallback is in use, since a guessable JWT secret lets anyone
forge a valid login token for any user). Constructed `AutoCommitPgUserRepository`,
`Argon2PasswordHasher`, `JwtTokenService(jwtSecret, std::chrono::hours(24))` as local variables in
`main()`, called `registerAuthRoutes(app, users, hasher, tokens)` — safe because `app`, `users`,
`hasher`, `tokens` all share `main()`'s stack frame and `app.run()` blocks for the server's entire
lifetime, so the captured references never dangle.

Built both `fitapp_server` and `fitapp_tests` clean. Then **live-verified manually** against the
real running server and real Postgres (not just tests): started the server via
`scripts/run-dev.ps1`, confirmed the `JWT_SECRET` warning fired, then `curl`'d the full flow —
register → login (right + wrong password) → `/me` (with + without token) → change password →
login with old password (`401`) → login with new password (`200`). Every step matched. Cleaned up
the manually-created test user from the dev database afterward, then stopped the server.

**`#18` is now complete.** Full suite: **70/70**.

---

## Files touched so far

| File | What |
|---|---|
| `Server/src/domain/User.h` | `User` struct |
| `Server/src/domain/PasswordPolicy.h` | `isPasswordStrong` — min 8 chars + ≥1 uppercase |
| `Server/src/ports/IUserRepository.h` | `IUserRepository` port + `DuplicateEmailError` |
| `Server/tests/support/InMemoryUserRepository.h` | Fake repo for tests |
| `Server/tests/test_password_policy.cpp` | 4 tests |
| `Server/tests/test_user_repository.cpp` | 5 tests |
| `Server/src/adapters/db/PgUserRepository.h/.cpp` | Real Postgres-backed `IUserRepository` |
| `Server/tests/test_pg_user_repository.cpp` | 5 integration tests (`DbTest`) |
| `Server/src/ports/IPasswordHasher.h` | Password hasher port |
| `Server/src/adapters/crypto/Argon2PasswordHasher.h/.cpp` | Real argon2id hasher, random salt per call |
| `Server/tests/test_password_hasher.cpp` | 3 tests |
| `Server/src/ports/ITokenService.h` | Token service port |
| `Server/src/adapters/crypto/JwtTokenService.h/.cpp` | Real HS256 JWT, configurable TTL |
| `Server/tests/test_token_service.cpp` | 4 tests |
| `Server/src/http/middleware/Authenticator.h` | `authenticate()` fn + `CurrentUser` (not Crow middleware) |
| `Server/tests/test_authenticator.cpp` | 5 tests |
| `Server/CMakeLists.txt` | Registered all new files; promoted auth deps onto `fitapp_server` |
| `Server/Dockerfile` | Added argon2/OpenSSL dev + runtime packages |
| `Server/src/app/RegisterUser.h` | First `app/` service: hash, save, issue token |
| `Server/tests/test_register_user.cpp` | 3 tests (happy path, weak password, duplicate email) |
| `Server/src/domain/PasswordPolicy.h` | + `WeakPasswordError`, `requireStrongPassword()` |
| `Server/src/app/LoginUser.h` | Find user, verify password, issue token; `InvalidCredentialsError` |
| `Server/tests/test_login_user.cpp` | 3 tests (happy path, wrong password, unknown email) |
| `Server/src/ports/IUserRepository.h` | + `findById`, `update` |
| `Server/tests/support/InMemoryUserRepository.h` | + `findById`, `update` |
| `Server/src/adapters/db/PgUserRepository.h/.cpp` | + `findById`, `update`, `rowToUser()` helper |
| `Server/tests/test_user_repository.cpp` | + 3 tests |
| `Server/tests/test_pg_user_repository.cpp` | + 3 tests |
| `Server/src/app/ChangePassword.h` | Change password while authenticated |
| `Server/tests/test_change_password.cpp` | 3 tests (happy path, wrong current, weak new) |
| `Server/src/http/dto/AuthDto.h` | Request parsing + response building for all 4 endpoints |
| `Server/src/http/controllers/AuthController.h/.cpp` | `registerAuthRoutes` — all 4 routes |
| `Server/tests/test_auth_controller.cpp` | 14 HTTP-level tests (`HttpClient`) |
| `Server/src/adapters/db/AutoCommitPgUserRepository.h/.cpp` | Production repo: connection-per-call |
| `Server/tests/test_auto_commit_pg_user_repository.cpp` | 2 tests (manual cleanup, not `DbTest`) |
| `Server/src/main.cpp` | Wired `JWT_SECRET`, real adapters, `registerAuthRoutes` — composition root |

---

## What's next

`#16`, `#17`, and `#18` are all complete — the whole backend of Slice 1 (Auth) is built, tested
(70/70), and live-verified against the real server and database. Only **`#24` (Client auth UI)**
remains: register/login forms, storing the token, and an authenticated `fetch` wrapper so future
protected endpoints (Slice 2+) can attach the token automatically. That's React/TypeScript work,
a different stack from everything built so far in this doc.
