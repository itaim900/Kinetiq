# Domain review fixes — validation limits, real calendar, shared error base (design)

**Date:** 2026-11-01 · **Status:** Approved (brainstorming) · **Branch:** `domain-review-fixes`
**Source:** full server review, [`docs/reviews/2026-10-26-code-review.md`](../../reviews/2026-10-26-code-review.md)
findings #6, #17 (password cap only), #19, #21 (number limits only), plus the domain notes from the
file-by-file walkthrough (duplicate error classes, `WeakPasswordError` without a field).

## Decisions
| # | Decision |
|---|---|
| D1 | **Password length 8–64 characters** (NIST SP 800-63B: allow at least 64). Owner first proposed 20; 64 chosen so password managers and passphrases work. Still requires one uppercase letter. |
| D2 | Password length counts **characters (Unicode code points), not bytes** — a Hebrew letter is 2 bytes in UTF-8 but one character. |
| D3 | `weekly_workout_goal` **0–21** (at most 3 workouts a day × 7). |
| D4 | `available_time_minutes` **1–300** (5 hours). |
| D5 | Birth date must be a **real calendar date**: strict `DDDD-DD-DD` digits, month 1–12, true days per month incl. leap years; year range 1900–2100 unchanged; still no "not in the future" check. |
| D6 | New base class **`domain::ValidationError`** (field + message). `InjuryValidationError` and `ProfileValidationError` become one-line subclasses (inheriting constructors); `WeakPasswordError` also derives from it and gains a field. |
| D7 | The policy still reports the **first** invalid field only (owner's choice). |
| D8 | Limits are enforced in **three layers**: domain policy (authority), client Zod (instant feedback), DB `CHECK` (migration `0007`). |

## 1. `domain/ValidationError.h` (new)
```cpp
class ValidationError : public std::runtime_error {
public:
    ValidationError(std::string field, const std::string& message);
    const std::string& field() const;   // wire name, e.g. "weight_kg"
};
```
- `InjuryPolicy.h`: `class InjuryValidationError : public ValidationError { public: using ValidationError::ValidationError; };`
- `ProfilePolicy.h`: same for `ProfileValidationError`.
- Existing `catch` sites and tests keep working unchanged; a `catch (const ValidationError&)` now
  catches all three types.

## 2. `PasswordPolicy.h`
- Constants `kMinPasswordLength = 8`, `kMaxPasswordLength = 64`.
- `detail::characterCount(const std::string&)`: counts bytes that are **not** UTF-8 continuation
  bytes (`(c & 0xC0) != 0x80`).
- `isPasswordStrong(pw)`: `8 <= characterCount(pw) <= 64` and at least one ASCII uppercase letter.
- `WeakPasswordError : ValidationError`, constructed with `(field, message)`.
- `requireStrongPassword(pw, field = "password")` throws with one of two messages:
  - too long → `"<field> must be at most 64 characters"`
  - otherwise → `"<field> must be at least 8 characters and include an uppercase letter"`
- `ChangePassword` calls `requireStrongPassword(newPassword, "new_password")`; `RegisterUser` uses
  the default.

## 3. `ProfilePolicy.h`
- Constants `kMaxWeeklyWorkouts = 21`, `kMaxSessionMinutes = 300`.
- `weekly_workout_goal`: must be `0..21` → message `"weekly_workout_goal must be between 0 and 21"`.
- `available_time_minutes`: must be `1..300` → `"available_time_minutes must be between 1 and 300"`.
- `detail::isPlausibleBirthDate` is replaced by `detail::isRealCalendarDate(s)`:
  1. length 10, `s[4] == '-'`, `s[7] == '-'`, every other character a digit (`'0'..'9'`);
  2. year 1900–2100, month 1–12;
  3. day 1..`daysInMonth(y, m)`; February has 29 days when
     `y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)`.

## 4. Migration `0007_profile_limits.sql`
Replace the two inline checks from `0002` (Postgres auto-named them `users_weekly_workout_goal_check`
and `users_available_time_minutes_check`; verify the names before writing the migration):
```sql
ALTER TABLE users
  DROP CONSTRAINT users_weekly_workout_goal_check,
  ADD  CONSTRAINT users_weekly_workout_goal_check
       CHECK (weekly_workout_goal IS NULL OR weekly_workout_goal BETWEEN 0 AND 21),
  DROP CONSTRAINT users_available_time_minutes_check,
  ADD  CONSTRAINT users_available_time_minutes_check
       CHECK (available_time_minutes IS NULL OR available_time_minutes BETWEEN 1 AND 300);
```
Before applying: check the dev DB has no rows outside the new limits (otherwise the migration fails
and the server refuses to start — the intended fail-closed behaviour). `birth_date` needs no change
(the `DATE` type already rejects impossible dates).

## 5. HTTP, client, docs
- **`AuthController`:** `WeakPasswordError` → `422` with `errors: { "<field>": [message] }` and
  `detail` = the same specific message (the register screen displays `detail`). Uses a variant of
  `validationErrorResponse(field, message, detail)`.
- **`profileSchema.ts`:** `weekly_workout_goal` whole number 0–21 ("Use a whole number from 0 to
  21"); `available_time_minutes` whole number 1–300 ("Use a whole number from 1 to 300").
- **Register form:** unchanged (no client rules today; it shows the server's message). The HTML
  `maxLength` attribute is deliberately **not** used — it silently truncates a pasted password.
- **`docs/02-api-contract.md`:** password policy 8–64 characters; register / change-password 422
  names the field; profile limits for the two number fields; new test cases.

## Testing (TDD — red first)
- `test_password_policy`: 7 chars ✗, 8 ✓, 64 ✓, 65 ✗; no uppercase ✗; 40 Hebrew letters + `A` ✓
  (would be ✗ if bytes were counted); `WeakPasswordError` carries the field; too-long message.
- `test_profile_policy`: workouts 21 ✓ / 22 ✗ / -1 ✗; minutes 300 ✓ / 301 ✗ / 0 ✗;
  dates `2000-02-29` ✓, `2024-02-29` ✓, `1900-02-29` ✗, `2023-02-29` ✗, `2001-02-31` ✗,
  `2001-04-31` ✗, `2001-+1-05` ✗, `2001-1 -05` ✗.
- A test that `catch (const ValidationError&)` catches `InjuryValidationError`,
  `ProfileValidationError` and `WeakPasswordError`, with `field()` intact.
- `test_change_password`: weak new password → error field `new_password`.
- `test_auth_controller`: weak / too-long password → `422` with `errors.password`; detail is the
  specific message.
- `test_pg_user_repository` (DB): writing `weekly_workout_goal = 22` or
  `available_time_minutes = 301` directly violates a `CHECK`.
- Client `profileSchema` / `ProfileForm` tests: 22 workouts and 301 minutes show the new messages.

## Out of scope
Length limits on notes / display name / email, the email format check, strict JSON number parsing,
honest 500s — all remain in the data-transport hardening pass (review findings #1–#10).
