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

## Suggested order
1. **#1 + #7 + #2** together — a typed JSON-reading helper with field-named errors, and honest
   500s. This is the core of "data transport hardening".
2. **#3 + #6 + #10** — complete the validation rules on the server, mirrored in Zod.
3. **#4 + #5** — make the wire format match storage and the docs (rounding, ISO timestamps).
4. **#8 + #9** — tidy the error plumbing on both sides.
5. **#11 + #12 + #13** → backlog.
