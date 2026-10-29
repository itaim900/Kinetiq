# Code review — after Slice 2 (2026-10-26)

**Scope:** every code file in `Server/src` (~2,800 lines C++) and `Client/src` (~1,950 lines TS),
with emphasis on **DTOs and data transport** — how data travels between Postgres, C++ objects,
JSON, TypeScript and the screen. Reviewed on `main` right after PR #41.

## How data travels (for orientation)

```
Postgres row ──rowToX()──▶ domain struct ──service──▶ domain struct ──toJson()──▶ JSON ──fetch──▶ TS type ──▶ screen
(Pg…Repository)            (User, Injury…)  (app/)                    (http/dto/)        (api.ts)   (features/)

screen ──form values──▶ Zod rules + toXRequest() ──JSON──▶ parseX() ──▶ request struct ──service──▶ domain struct ──▶ Pg…Repository
       (React Hook Form)   (*Schema.ts)                    (http/dto/)    (app/)          (validate)
```
Each arrow is a translation, and every translation is a place where names, types, `null`s or
rounding can drift.

## What's already good
- **The DTO boundary is clean:** `password_hash` never leaves the server; the user id always comes
  from the token, never from the body; one `xToJson()` per shape.
- **Nulls are explicit:** unset fields serialize as JSON `null`; empty lists as `[]`, never `null`.
- **Consistent snake_case on the wire**, camelCase in C++/TS internals, translated in one place.
- **JSON Merge Patch** for profile updates is a real standard with clear semantics.
- **Client types mirror the server** closely; `AddInjuryRequest` as a union makes the two request
  shapes impossible to mix at compile time.
- Rules live in testable, pure places (`*Policy.h`, `*Schema.ts`).

## Findings

Severity: 🔴 high (wrong data or misleading behaviour today) · 🟠 medium (a real gap, limited
impact today) · 🟡 low (maintainability / future-proofing).

### 🔴 1. Integer fields are parsed leniently — wrong values are saved with a 200 OK
Crow's `rvalue::i()` accepts **strings** as well as numbers, and converts through a `stringstream`
**without checking whether the read succeeded**; we then `static_cast<int>`. Verified with a
throwaway program against the vendored `crow_all.h`:

| JSON value | Server reads |
|---|---|
| `2.5` | `2` (decimal dropped) |
| `"3"` | `3` (string accepted) |
| `"abc"` | **`0`** (garbage becomes zero) |
| `1e3` | `1` |
| `4294967297` | `1` (after `static_cast<int>` wraps) |

So `PATCH /users/me {"weekly_workout_goal": "abc"}` stores **0** and returns 200. Affects
`weekly_workout_goal`, `available_time_minutes` (`ProfileDto.h`), `condition_id` and every
`muscle_ids` item (`InjuryDto.h`). The client never sends such values, but the server is the
authority and must not trust it.
**Fix:** a small typed-parsing helper in the DTO layer (`readInt(body, "field", min, max)`) that
requires `type::Number`, checks the text is a whole number in range, and throws a field-named
error. Use it everywhere instead of raw `.i()`.

### 🔴 2. Every unexpected exception becomes "422 Validation Failed"
Controllers end with `catch (const std::exception&) { return 422 "One or more fields are missing
or invalid." }`. That catch also swallows **server faults** — Postgres down, a constraint the
validators missed, a bug — and reports them to the client as the user's mistake, with nothing logged.
Example: `birth_date: "2001-02-31"` passes our validator (see #6), Postgres rejects the `DATE`, and
the user is told their input is "missing or invalid" with no field.
**Fix:** separate *parse* errors (→ 422 with a field, see #1/#7) from *everything else*
(→ 500 "Internal error", and log it with `CROW_LOG_ERROR`).

### 🟠 3. No length limits on free text (server side)
`display_name`, `email`, `medications`, injury `notes` have no maximum on the server. The client
caps injury notes at 500 characters, but the server doesn't — the rule exists on one side only,
which is backwards ("the server is the authority"). Anyone with the API can store megabytes.
**Fix:** limits in the domain policies (e.g. email ≤ 254, display name 1–100, medications ≤ 1000,
notes ≤ 500), mirrored in the Zod schemas; plus `display_name` must not be empty (today it can be).

### 🟠 4. The PATCH response can disagree with what was stored
`height_cm` is `NUMERIC(5,1)` and `weight_kg` is `NUMERIC(5,2)` in Postgres, so the database rounds;
but `UpdateProfile` returns the **in-memory** object, not what was saved. `PATCH {"height_cm":
180.16}` answers `180.16`; the next `GET` says `180.2`. Our client rounds first, so the UI never
shows it — but the API contradicts itself.
**Fix:** round in the domain to the column precision before saving (simplest), or read the row
back after saving.

### 🟠 5. Timestamps aren't ISO 8601
`created_at` is sent as Postgres's default text, `2026-10-26 18:13:50.778312+00` (space instead of
`T`, offset `+00` instead of `Z`/`+00:00`). Docs and comments promise ISO 8601. Some browsers'
`new Date(...)` reject this format; nothing parses it yet, so it's latent. The MSW mock returns
proper ISO (`2026-10-22T10:00:00Z`), so the tests can't see the difference — a **mock drift** (#12).
**Fix:** format in SQL (`to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')`) or
in C++, in one shared helper.

### 🟠 6. Birth-date rules differ between client and server
Server `isPlausibleBirthDate` checks only shape and ranges (`day ≤ 31` for every month, years
1900–2100), so it accepts `2001-02-31` — which Postgres then rejects (→ #2). The client's
`isValidDate` correctly rejects it. Same rule, two languages, two answers.
**Fix:** check the real calendar on the server (days per month, leap years); consider rejecting
future dates.

### 🟠 7. Type errors come back without a field name
A wrong *type* (e.g. `"severity": 5`, `"weight_kg": "heavy"`) throws inside `parseX()` and lands in
the generic catch: 422 **without** the `errors` map, so the client can't put the message under the
field. Only *value* errors (from the policies) are field-named.
**Fix:** the typed helpers from #1 (`readString`, `readNumber`, `readInt`, `readBool`,
`readIdList`) throw a field-named parse error, mapped by one `validationErrorResponse(field, …)`.

### 🟡 8. Error-response helpers are scattered and repeated
`errorResponse` lives in `AuthDto.h`, `validationErrorResponse` in `ProfileDto.h`;
`InjuryController.cpp` includes both. The "401 Missing, invalid, or expired token" block is
copy-pasted 9 times. Also, RFC 9457 (which the contract cites) expects `Content-Type:
application/problem+json` and a URI `type`; we send `application/json` and `"validation_error"`.
**Fix:** one `http/dto/Problem.h` (`problem(status, type, title, detail)`, `validationProblem`,
`unauthenticated()`), plus a `requireUser(req, tokens)` helper. Decide on the content type and
document it in the contract.

### 🟡 9. The client has two error paths
`registerUser` / `loginUser` / `fetchMe` use the old `postJson` + plain `Error`; everything else
uses `authFetch` + `ApiError` with per-field errors. Register can't show field errors, and there
are two places to maintain.
**Fix:** a single `requestJson()` underneath both (with or without the token), always throwing `ApiError`.

### 🟡 10. Registration accepts any email text
The server stores `"not-an-email"` (the client's `type="email"` isn't enforced because forms use
`noValidate`). Note also that `409 "Email is already registered"` reveals which emails exist —
the same thing the login design avoids (the login timing side-channel is already a documented,
accepted v1 risk).
**Fix:** a minimal email shape check on the server (something@something.tld, length ≤ 254); decide
consciously about registration enumeration (common approach: accept it, rate-limit later).

### 🟡 11. One connection per repository call, and N+1 queries
Every `AutoCommit…` call opens a new Postgres connection. Adding an unlisted injury with 3 muscles
opens ~6 connections (region check, one per muscle, insert); listing conditions runs 1 + 17
queries; listing injuries runs 1 + N. Fine at today's size, but Slice 3 adds an exercise catalog.
**Fix later:** a connection pool, `findMuscles(ids)` in one query, and JOIN-based list queries.
Add to the backlog; measure before optimising.

### 🟡 12. MSW mocks re-implement the server by hand
`mocks/handlers.ts` re-creates server behaviour (catalog lookup, muscle resolution, timestamps)
without the server's validation. It's useful, but it already drifted (#5) and will drift more.
**Fix:** keep mocks thin; share fixture JSON captured from the real server; consider a few
contract tests that run the client's API functions against the real server in CI.

### 🟡 13. Smaller items
- `GET /users/me` is fetched twice when the Profile tab opens (`UnitsProvider` + `ProfilePage`).
- `Profile.sex` / `experience_level` are typed `string | null` in TS; unions (like `Severity`) would
  catch typos at compile time.
- Ids are JS numbers; Postgres `BIGINT` ids above 2^53 would lose precision (theoretical for us).
- `Migrator` has no advisory lock — two servers starting at once could both apply a migration
  (we only run one).

---

# Part 2 — full server review, file by file (2026-10-29)

**Scope:** every file under `Server/src` (domain, ports, app, adapters, http, `main.cpp`, config),
plus `Server/migrations`, `Server/tests/support` and `docker-compose.prod.yml`. Findings 1–13 above
still stand; these are new.

## What's good, server-wide
- **The architecture holds:** no file in `domain/`, `ports/` or `app/` includes Crow or libpqxx.
  Every dependency points inward, exactly as the READMEs promise.
- **No SQL injection anywhere:** every query uses `$1`-style parameters (`pqxx::params`); the one
  string-built query (`Migrator`) quotes its value.
- **Crypto is done right:** argon2id with a random OpenSSL salt and parameters embedded in the hash;
  JWT verification pins HS256 (no `alg: none` trick), checks the issuer and expiry, and never throws.
- **Privacy rules are enforced in one place each:** `password_hash` is never serialized; the user id
  always comes from the token; someone else's injury is a 404, never a 403.
- **Fail-closed startup:** a broken migration stops the server instead of serving a half-built schema.
- **Comments explain *why*,** not just what — a big help for a learning project.

## New findings

### 🔴 14. The production compose file doesn't set `JWT_SECRET`
`docker-compose.prod.yml` sets the DB variables but not `JWT_SECRET`, so the containerized stack
signs logins with the fallback `"dev-only-insecure-secret-change-in-production"`, which is public in
this repository. Anyone who reads `main.cpp` can forge a valid token for **any user id**. The only
signal is a warning line in the server log.
**Fix:** `JWT_SECRET: ${JWT_SECRET:?set JWT_SECRET}` in the prod compose (compose refuses to start
without it); in the server, refuse to start with the fallback unless running locally
(`BIND_ADDR=127.0.0.1`), and reject secrets shorter than 32 characters.

### 🟠 15. The same failure gets different answers depending on the route (sharpens #2)
Crow already turns an **uncaught** exception into `500` + an error log. Routes **without** a
`try` (`GET /users/me/injuries`, the three catalog routes, `GET /auth/me`, `GET /users/me`) get that
honest 500 when Postgres is down. Routes **with** the `catch (std::exception&)` (register, login,
PATCH profile, POST/PATCH/DELETE injury) turn the very same outage into
`422 "fields are missing or invalid"`, with no log.
**Fix (part of #2):** once parsing throws typed, field-named errors (#1/#7), delete the catch-alls
and let Crow's 500 handler (or one shared handler) deal with the unexpected.

### 🟠 16. Old logins stay valid after a password change
JWTs are stateless: `ChangePassword` doesn't touch existing tokens, so a stolen token keeps working
for up to 24 h even after the user changes their password. Usually that's the whole point of
changing it.
**Fix:** for v1, document it as a known limitation in `02-api-contract.md`. Properly: a
`password_changed_at` (or `token_version`) column checked in `authenticate()` (one DB read per
request), or short-lived access tokens + refresh tokens (backlog, with "forgot password").

### 🟠 17. No limits on expensive or large requests
- **Login/register have no rate limit:** unlimited password guessing per account.
- **Each hash costs 64 MiB of RAM** (argon2 memory cost). Crow runs one thread per CPU core, so a
  burst of logins can use hundreds of MB on this 8 GB machine.
- **No password length cap** (argon2 accepts megabytes) and **no request-body cap** in Crow
  (`nginx` in the prod stack defaults to 1 MB, but port 8080 is exposed directly too).
**Fix now (cheap):** a maximum password length (e.g. 128) in `PasswordPolicy`, and a body-size check
before `json::load`. **Later (backlog):** rate limiting per IP / per email.

### 🟡 18. Dates come out in whatever format Postgres is set to (extends #5)
`birth_date` and `created_at` are read with `as<std::string>()`, so their text depends on the DB
session's `DateStyle` setting. It's ISO-like by default, so fine today, but it's an invisible
dependency.
**Fix (with #5):** format dates explicitly in SQL (`to_char(...)`) in one shared place.

### 🟡 19. The birth-date parser also accepts odd shapes (extends #6)
`sscanf("%2d")` accepts a sign or a space, so `"2001-+1-05"` passes the server check (then Postgres
rejects it → the generic 422 from #2).
**Fix (with #6):** parse digits strictly, then check the real calendar.

### 🟡 20. Unknown or misspelled fields are silently ignored
`PATCH /users/me {"weigth_kg": 80}` (typo) answers `200 OK` and changes nothing; the client thinks
it saved.
**Fix:** reject unknown keys with a field-named 422 (`"weigth_kg": ["unknown field"]`), at least on
PATCH bodies.

### 🟡 21. A few value limits are missing (extends #3)
- `weekly_workout_goal` and `available_time_minutes` have no upper bound (2,000,000,000 is accepted).
- `user_injuries.severity` is nullable in the schema but required in code; `rowToInjury` turns a
  NULL into `""`, which would then fail validation the next time that injury is edited.
**Fix:** sensible maximums (e.g. ≤ 14 workouts/week, ≤ 600 minutes) in `ProfilePolicy` + Zod, and a
new migration adding the matching `CHECK`s and `severity NOT NULL`.

### 🟡 22. Profile history stores unrounded values (part of #4)
History records the in-memory value (`72.456`) while the users table keeps the rounded `72.46`, so
history and profile disagree. Fixed automatically by rounding in the domain (#4).

### 🟡 23. `Migrator` doesn't notice an edited migration
An already-applied file that was changed later is silently skipped (only its name is recorded).
**Fix later:** store a checksum per migration and refuse to start on a mismatch (backlog).

### 🟡 24. Fakes and real adapters can drift apart
`InMemoryInjuryRepository` invents an ISO `createdAt` (Postgres doesn't produce one, see #5);
`InMemoryUserRepository::update` never throws `DuplicateEmailError` (Postgres does). The "same
contract" is written twice, once in the fake tests and once in the Pg tests.
**Fix later:** one shared contract test suite (GoogleTest `TYPED_TEST`) run against both the fake
and the real adapter (backlog, pairs with #12).

### 🟡 25. Small items
- The `Bearer` prefix check is case-sensitive (the HTTP spec says the scheme name isn't).
- `/health` is public, opens a new DB connection on every call, and sends `CORS: *`.
- Port `8080` is hard-coded (everything else comes from env vars).
- `PATCH /auth/password` answers `200` with an empty body (`204 No Content` says that more precisely).
- Repository `update()` silently does nothing for a missing id (the services check first, so it's
  only reachable in a race).

## Suggested order (updated)
0. **#14** — tiny, and it's a security hole: do it first.
1. **#1 + #7 + #2 + #15 + #20** — typed JSON reading with field-named errors, reject unknown
   fields, honest 500s. The core of "data transport hardening".
2. **#3 + #21 + #6 + #19 + #10 + #17 (caps only)** — complete the validation rules on the server,
   mirrored in Zod, plus password and body-size caps.
3. **#4 + #22 + #5 + #18** — the wire format matches storage and the docs (rounding, ISO dates).
4. **#8 + #9** — tidy the error plumbing on both sides.
5. **Backlog:** #11, #12, #13, #16 (document now), #17 (rate limiting), #23, #24, #25.
