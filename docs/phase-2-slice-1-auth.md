# Phase 2, Slice 1 — Auth: Step-by-Step Build Log

**Status:** 🟡 IN PROGRESS (started 2026-07-12)
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
- [ ] argon2 password hasher wrapper — #17
- [ ] JWT issue/verify service — #17
- [ ] Authenticator function (`Authorization: Bearer` → `CurrentUser` or 401) — #17
- [ ] Promote auth deps from `fitapp_tests` to `fitapp_server` + `Server/Dockerfile` — #17
- [ ] `POST /auth/register` (handler + DTOs + HTTP tests) — #18
- [ ] `POST /auth/login` (handler + DTOs + HTTP tests) — #18
- [ ] `GET /auth/me` (handler + HTTP tests) — #18
- [ ] `PATCH /auth/password` (handler + HTTP tests) — #18 (added to scope 2026-07-12)
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

### 2026-07-12 — Change password (while logged in): added to Slice 1
No email infra needed — the user is already authenticated, submits current + new password, the
service re-verifies and re-hashes. New endpoint: `PATCH /auth/password` 🔒. Full spec + test
cases in `02-api-contract.md`.

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
| `Server/CMakeLists.txt` | Registered all new test/source files |

---

## What's next

Per the checklist above: `PgUserRepository` (the real Postgres half of #16), then #17 (argon2 +
JWT + authenticator function — **not** Crow middleware, see `phase-2-plan.md` §8 for why), then
#18's HTTP-level tests and handlers for all four endpoints (register, login, me,
change-password), written test-first against `02-api-contract.md`'s test cases the same way
Steps 2–3 were.
