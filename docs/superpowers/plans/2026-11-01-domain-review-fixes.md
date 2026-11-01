# Domain Review Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tighten domain validation (password 8–64 chars, 0–21 workouts, 1–300 minutes, real calendar
birth dates), share one `ValidationError` base, and harden password hashing (strong `PasswordHash`
type, constant-cost login failures, a fast fake hasher for tests).

**Architecture:** All rules live in header-only `Server/src/domain/` policies; the HTTP layer maps
`ValidationError` subclasses to 422 field errors; the DB mirrors the number limits with `CHECK`s
(migration 0007) and the client mirrors them in Zod. `PasswordHash` is a domain value type used by
the `IPasswordHasher` port.

**Tech Stack:** C++20, GoogleTest, Crow, libpqxx, PostgreSQL 16, React/TypeScript, Zod 4, Vitest.

**Spec:** [`docs/superpowers/specs/2026-11-01-domain-review-fixes-design.md`](../specs/2026-11-01-domain-review-fixes-design.md)

## Global Constraints
- Password: 8–64 **characters** (Unicode code points), at least one uppercase ASCII letter.
- `weekly_workout_goal`: 0–21. `available_time_minutes`: 1–300.
- Birth date: strict `YYYY-MM-DD` digits, years 1900–2100, real days per month incl. leap years.
- Messages: `"<field> must be at most 64 characters"`, `"<field> must be at least 8 characters and
  include an uppercase letter"`, `"weekly_workout_goal must be between 0 and 21"`,
  `"available_time_minutes must be between 1 and 300"`.
- Build with `cmake --build --preset default -j 2` (8 GB machine). New `.cpp` → add to
  `Server/CMakeLists.txt` by hand. Every commit green through the git hooks.
- Docker Postgres must be up so DB tests don't skip.

---

### Task 1: `ValidationError` base class

**Files:**
- Create: `Server/src/domain/ValidationError.h`
- Modify: `Server/src/domain/InjuryPolicy.h:10-20`, `Server/src/domain/ProfilePolicy.h:12-23`
- Create test: `Server/tests/test_validation_error.cpp` (+ add to `kinetiq_tests` in `Server/CMakeLists.txt`)

**Interfaces:** Produces `kinetiq::domain::ValidationError(std::string field, const std::string& message)`
with `const std::string& field() const`; `InjuryValidationError`/`ProfileValidationError` derive from it
with `using ValidationError::ValidationError;`.

- [ ] **Step 1: Failing test** — `test_validation_error.cpp`:
```cpp
#include <gtest/gtest.h>
#include "domain/InjuryPolicy.h"
#include "domain/ProfilePolicy.h"
#include "domain/ValidationError.h"
#include <functional>
#include <string>

using namespace kinetiq::domain;

namespace {
// Returns the field of the ValidationError `thrower` raises, or "<not caught>".
std::string fieldCaughtAsBase(const std::function<void()>& thrower) {
    try { thrower(); } catch (const ValidationError& e) { return e.field(); }
    return "<not caught>";
}
}  // namespace

TEST(ValidationError, BaseCatchesInjuryAndProfileErrors) {
    EXPECT_EQ(fieldCaughtAsBase([] { throw InjuryValidationError("severity", "bad"); }), "severity");
    EXPECT_EQ(fieldCaughtAsBase([] { throw ProfileValidationError("weight_kg", "bad"); }), "weight_kg");
}

TEST(ValidationError, KeepsTheMessage) {
    const ValidationError e("height_cm", "too tall");
    EXPECT_STREQ(e.what(), "too tall");
}
```
- [ ] **Step 2:** Build → fails (`ValidationError.h` not found).
- [ ] **Step 3: Implement** `ValidationError.h`:
```cpp
#pragma once
#include <stdexcept>
#include <string>
#include <utility>

namespace kinetiq::domain {

// Base of every "this field is invalid" error. Carries the field's wire name
// (e.g. "weight_kg") so the HTTP layer can build the RFC 9457 `errors` map.
class ValidationError : public std::runtime_error {
public:
    ValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

}  // namespace kinetiq::domain
```
  Replace the class bodies in `InjuryPolicy.h` / `ProfilePolicy.h` with
  `class XValidationError : public ValidationError { public: using ValidationError::ValidationError; };`
  and `#include "domain/ValidationError.h"`.
- [ ] **Step 4:** Build + run `kinetiq_tests` → all pass.
- [ ] **Step 5:** Commit `refactor(domain): shared ValidationError base for field errors`.

### Task 2: Password policy 8–64 characters, `WeakPasswordError` names its field

**Files:**
- Modify: `Server/src/domain/PasswordPolicy.h`, `Server/src/app/ChangePassword.h:33`,
  `Server/src/http/dto/ProfileDto.h:70-79` (`validationErrorResponse` gains a `detail` param),
  `Server/src/http/controllers/AuthController.cpp` (both `WeakPasswordError` catches)
- Tests: `test_password_policy.cpp`, `test_validation_error.cpp`, `test_change_password.cpp`, `test_auth_controller.cpp`

**Interfaces:** Produces `kMinPasswordLength = 8`, `kMaxPasswordLength = 64`,
`detail::characterCount(const std::string&)`, `WeakPasswordError : ValidationError`,
`requireStrongPassword(const std::string& password, const std::string& field = "password")`,
`dto::validationErrorResponse(field, message, detail = "One or more fields are invalid.")`.

- [ ] **Step 1: Failing tests.** `test_password_policy.cpp` add:
```cpp
TEST(PasswordPolicy, AcceptsUpTo64Characters_Rejects65) {
    EXPECT_TRUE(isPasswordStrong(std::string(63, 'a') + "A"));   // 64
    EXPECT_FALSE(isPasswordStrong(std::string(64, 'a') + "A"));  // 65
}

TEST(PasswordPolicy, CountsCharactersNotBytes) {
    std::string hebrew;
    for (int i = 0; i < 40; ++i) hebrew += "א";  // 2 bytes each in UTF-8
    EXPECT_TRUE(isPasswordStrong(hebrew + "A"));  // 41 characters (81 bytes)
}

TEST(PasswordPolicy, WeakPasswordErrorNamesTheField) {
    try {
        requireStrongPassword("weak", "new_password");
        FAIL() << "expected WeakPasswordError";
    } catch (const WeakPasswordError& e) {
        EXPECT_EQ(e.field(), "new_password");
        EXPECT_STREQ(e.what(), "new_password must be at least 8 characters and include an uppercase letter");
    }
}

TEST(PasswordPolicy, TooLongPasswordHasItsOwnMessage) {
    try {
        requireStrongPassword(std::string(65, 'A'));
        FAIL() << "expected WeakPasswordError";
    } catch (const WeakPasswordError& e) {
        EXPECT_EQ(e.field(), "password");
        EXPECT_STREQ(e.what(), "password must be at most 64 characters");
    }
}
```
  `test_validation_error.cpp` add `EXPECT_EQ(fieldCaughtAsBase([] { throw WeakPasswordError("password", "bad"); }), "password");`
  (include `domain/PasswordPolicy.h`).
  `test_change_password.cpp` add a test: weak `new_password` → `WeakPasswordError` with `field() == "new_password"`.
  `test_auth_controller.cpp`: in `Register_WeakPassword_Returns422` and a new
  `Register_TooLongPassword_Returns422NamingPassword`, parse the body and assert
  `body["errors"]["password"][0]` is the policy message and `body["detail"]` equals it;
  in `ChangePassword_WeakNewPassword_Returns422` assert `body["errors"].has("new_password")`.
- [ ] **Step 2:** Build → fails (no 2-arg `WeakPasswordError`, no `kMaxPasswordLength`).
- [ ] **Step 3: Implement** `PasswordPolicy.h`:
```cpp
#include "domain/ValidationError.h"
// ...
constexpr std::size_t kMinPasswordLength = 8;
constexpr std::size_t kMaxPasswordLength = 64;

namespace detail {
// Characters, not bytes: in UTF-8 every byte of a character except the first
// looks like 10xxxxxx, so count the bytes that don't.
inline std::size_t characterCount(const std::string& s) {
    return static_cast<std::size_t>(std::count_if(
        s.begin(), s.end(), [](unsigned char c) { return (c & 0xC0) != 0x80; }));
}
}  // namespace detail

inline bool isPasswordStrong(const std::string& password) {
    const std::size_t length = detail::characterCount(password);
    if (length < kMinPasswordLength || length > kMaxPasswordLength) return false;
    return std::any_of(password.begin(), password.end(),
                       [](unsigned char c) { return std::isupper(c); });
}

class WeakPasswordError : public ValidationError {
public:
    using ValidationError::ValidationError;
};

inline void requireStrongPassword(const std::string& password,
                                  const std::string& field = "password") {
    if (isPasswordStrong(password)) return;
    if (detail::characterCount(password) > kMaxPasswordLength) {
        throw WeakPasswordError(field, field + " must be at most " +
                                           std::to_string(kMaxPasswordLength) + " characters");
    }
    throw WeakPasswordError(field, field + " must be at least " +
                                       std::to_string(kMinPasswordLength) +
                                       " characters and include an uppercase letter");
}
```
  `ChangePassword.h`: `requireStrongPassword(request.newPassword, "new_password");`.
  `ProfileDto.h`: add `const std::string& detail = "One or more fields are invalid."` as third parameter.
  `AuthController.cpp`: both catches become
  `catch (const kinetiq::domain::WeakPasswordError& e) { return dto::validationErrorResponse(e.field(), e.what(), e.what()); }`
  (include `http/dto/ProfileDto.h`).
- [ ] **Step 4:** Build + run → all pass.
- [ ] **Step 5:** Commit `feat(auth): password 8-64 characters; weak-password 422 names the field`.

### Task 3: Profile limits + real calendar birth date

**Files:** Modify `Server/src/domain/ProfilePolicy.h`; test `Server/tests/test_profile_policy.cpp`.

**Interfaces:** Produces `kMaxWeeklyWorkouts = 21`, `kMaxSessionMinutes = 300`,
`detail::isLeapYear(int)`, `detail::daysInMonth(int, int)`, `detail::isRealCalendarDate(const std::string&)`
(replaces `isPlausibleBirthDate`).

- [ ] **Step 1: Failing tests** (append):
```cpp
namespace {
bool profileValidWith(void (*change)(User&)) {
    User u = validProfile();
    change(u);
    try { requireValidProfile(u); return true; } catch (const ProfileValidationError&) { return false; }
}
bool birthDateOk(const std::string& date) {
    User u = validProfile();
    u.birthDate = date;
    try { requireValidProfile(u); return true; } catch (const ProfileValidationError&) { return false; }
}
}  // namespace

TEST(ProfilePolicy, WeeklyGoal_0To21) {
    EXPECT_TRUE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 0; }));
    EXPECT_TRUE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 21; }));
    EXPECT_FALSE(profileValidWith([](User& u) { u.weeklyWorkoutGoal = 22; }));
}

TEST(ProfilePolicy, AvailableTime_1To300) {
    EXPECT_TRUE(profileValidWith([](User& u) { u.availableTimeMinutes = 1; }));
    EXPECT_TRUE(profileValidWith([](User& u) { u.availableTimeMinutes = 300; }));
    EXPECT_FALSE(profileValidWith([](User& u) { u.availableTimeMinutes = 301; }));
}

TEST(ProfilePolicy, LimitMessagesNameTheRange) {
    User u = validProfile();
    u.weeklyWorkoutGoal = 22;
    try { requireValidProfile(u); FAIL(); } catch (const ProfileValidationError& e) {
        EXPECT_STREQ(e.what(), "weekly_workout_goal must be between 0 and 21");
    }
    u = validProfile();
    u.availableTimeMinutes = 301;
    try { requireValidProfile(u); FAIL(); } catch (const ProfileValidationError& e) {
        EXPECT_STREQ(e.what(), "available_time_minutes must be between 1 and 300");
    }
}

TEST(ProfilePolicy, BirthDate_MustBeARealCalendarDate) {
    EXPECT_TRUE(birthDateOk("2000-02-29"));   // divisible by 400 -> leap year
    EXPECT_TRUE(birthDateOk("2024-02-29"));   // divisible by 4 -> leap year
    EXPECT_TRUE(birthDateOk("2001-12-31"));
    EXPECT_FALSE(birthDateOk("1900-02-29"));  // century, not by 400 -> not leap
    EXPECT_FALSE(birthDateOk("2023-02-29"));
    EXPECT_FALSE(birthDateOk("2001-02-31"));
    EXPECT_FALSE(birthDateOk("2001-04-31"));  // April has 30 days
    EXPECT_FALSE(birthDateOk("2001-13-01"));
    EXPECT_FALSE(birthDateOk("2001-00-10"));
    EXPECT_FALSE(birthDateOk("2001-+1-05"));  // sign sneaking in
    EXPECT_FALSE(birthDateOk("2001-1 -05"));  // space sneaking in
    EXPECT_FALSE(birthDateOk("1899-12-31"));
}
```
- [ ] **Step 2:** Build + run → the new tests fail (22, 301, Feb 31, `+1` accepted).
- [ ] **Step 3: Implement** in `ProfilePolicy.h` (drop `<cstdio>`):
```cpp
constexpr int kMaxWeeklyWorkouts = 21;   // at most 3 workouts a day x 7 days
constexpr int kMaxSessionMinutes = 300;  // 5 hours

namespace detail {
inline bool isLeapYear(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }

inline int daysInMonth(int y, int m) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && isLeapYear(y)) ? 29 : kDays[m - 1];
}

// A real date in strict YYYY-MM-DD: digits only (no signs or spaces), years
// 1900-2100, and a day that exists in that month. Still no "not in the future"
// check (would make the rule depend on the clock; documented deferral).
inline bool isRealCalendarDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    const auto number = [&s](int from, int length) {
        int value = 0;
        for (int i = from; i < from + length; ++i) value = value * 10 + (s[i] - '0');
        return value;
    };
    const int y = number(0, 4), m = number(5, 2), d = number(8, 2);
    if (y < 1900 || y > 2100 || m < 1 || m > 12) return false;
    return d >= 1 && d <= daysInMonth(y, m);
}
}  // namespace detail
```
  and in `requireValidProfile`:
```cpp
if (u.weeklyWorkoutGoal && (*u.weeklyWorkoutGoal < 0 || *u.weeklyWorkoutGoal > kMaxWeeklyWorkouts)) {
    throw ProfileValidationError("weekly_workout_goal", "weekly_workout_goal must be between 0 and " +
                                                            std::to_string(kMaxWeeklyWorkouts));
}
if (u.availableTimeMinutes &&
    (*u.availableTimeMinutes < 1 || *u.availableTimeMinutes > kMaxSessionMinutes)) {
    throw ProfileValidationError("available_time_minutes",
                                 "available_time_minutes must be between 1 and " +
                                     std::to_string(kMaxSessionMinutes));
}
if (u.birthDate && !detail::isRealCalendarDate(*u.birthDate)) { /* same message as before */ }
```
- [ ] **Step 4:** Build + run → all pass.
- [ ] **Step 5:** Commit `feat(profile): 0-21 workouts, 1-300 minutes, real calendar birth dates`.

### Task 4: Migration 0007 — DB checks match the policy

**Files:** Create `Server/migrations/0007_profile_limits.sql`; test in `Server/tests/test_migrations.cpp`.

- [ ] **Step 1:** Confirm constraint names and that no dev row breaks the new limits:
  `docker exec kinetiq-postgres psql -U kinetiq -d kinetiq -tAc "select conname from pg_constraint where conrelid='users'::regclass and contype='c'; select count(*) from users where weekly_workout_goal>21 or available_time_minutes>300;"`
  Expected: names `users_weekly_workout_goal_check`, `users_available_time_minutes_check`; count `0`.
- [ ] **Step 2: Failing DB tests** (DbTest fixture):
```cpp
TEST_F(DbTest, UsersTable_RejectsMoreThan21WeeklyWorkouts) {
    if (!available()) GTEST_SKIP() << "no test database";
    EXPECT_THROW(tx().exec("INSERT INTO users (email, password_hash, weekly_workout_goal) "
                           "VALUES ('limits1@example.com', 'x', 22)"),
                 pqxx::check_violation);
}

TEST_F(DbTest, UsersTable_RejectsSessionsOver300Minutes) {
    if (!available()) GTEST_SKIP() << "no test database";
    EXPECT_THROW(tx().exec("INSERT INTO users (email, password_hash, available_time_minutes) "
                           "VALUES ('limits2@example.com', 'x', 301)"),
                 pqxx::check_violation);
}

TEST_F(DbTest, UsersTable_AcceptsTheLimitsThemselves) {
    if (!available()) GTEST_SKIP() << "no test database";
    EXPECT_NO_THROW(tx().exec("INSERT INTO users (email, password_hash, weekly_workout_goal, "
                              "available_time_minutes) VALUES ('limits3@example.com', 'x', 21, 300)"));
}
```
- [ ] **Step 3:** Run → the first two fail (no exception).
- [ ] **Step 4: Migration:**
```sql
-- 0007: profile limits match the domain ProfilePolicy (spec 2026-11-01):
-- at most 21 workouts a week (3 a day), sessions 1-300 minutes.
ALTER TABLE users
  DROP CONSTRAINT users_weekly_workout_goal_check,
  ADD  CONSTRAINT users_weekly_workout_goal_check
       CHECK (weekly_workout_goal IS NULL OR weekly_workout_goal BETWEEN 0 AND 21),
  DROP CONSTRAINT users_available_time_minutes_check,
  ADD  CONSTRAINT users_available_time_minutes_check
       CHECK (available_time_minutes IS NULL OR available_time_minutes BETWEEN 1 AND 300);
```
- [ ] **Step 5:** Run (the test DB migrates on suite start) → all pass, 0 skipped. Commit `feat(db): migration 0007 profile limit checks`.

### Task 5: Client Zod mirrors the limits

**Files:** Modify `Client/src/features/profile/profileSchema.ts`; test `profileSchema.test.ts`.

- [ ] **Step 1: Failing test:**
```ts
it('limits workouts to 0-21 and minutes to 1-300', () => {
  expect(errorPaths('metric', { weekly_workout_goal: '21' })).toEqual([]);
  expect(errorPaths('metric', { weekly_workout_goal: '22' })).toEqual(['weekly_workout_goal']);
  expect(errorPaths('metric', { available_time_minutes: '300' })).toEqual([]);
  expect(errorPaths('metric', { available_time_minutes: '301' })).toEqual(['available_time_minutes']);
});
```
- [ ] **Step 2:** `npx vitest run src/features/profile` → fails.
- [ ] **Step 3: Implement:**
```ts
const WEEKLY_WORKOUTS_MAX = 21;
const SESSION_MINUTES_MAX = 300;
// ...
weekly_workout_goal: z
  .string()
  .refine(
    blankOr((n) => Number.isInteger(n) && n >= 0 && n <= WEEKLY_WORKOUTS_MAX),
    `Use a whole number from 0 to ${WEEKLY_WORKOUTS_MAX}`,
  ),
available_time_minutes: z
  .string()
  .refine(
    blankOr((n) => Number.isInteger(n) && n >= 1 && n <= SESSION_MINUTES_MAX),
    `Use a whole number from 1 to ${SESSION_MINUTES_MAX}`,
  ),
```
  Update any test asserting the old wording ("Use a whole number, 0 or more" / "above 0").
- [ ] **Step 4:** `npm test` → all pass. Commit `feat(client): profile form mirrors workout and minutes limits`.

### Task 6: `PasswordHash` strong type

**Files:**
- Create: `Server/src/domain/PasswordHash.h`
- Modify: `domain/User.h`, `ports/IPasswordHasher.h`, `adapters/crypto/Argon2PasswordHasher.{h,cpp}`,
  `adapters/db/PgUserRepository.cpp` (lines 11, 37, 77); tests that assign or compare `passwordHash`.

**Interfaces:** Produces `kinetiq::domain::PasswordHash` (`explicit PasswordHash(std::string)`,
`const std::string& encoded() const`, `operator==`); port
`PasswordHash hash(const std::string&)`, `bool verify(const PasswordHash&, const std::string&)`.

- [ ] **Step 1: Failing test** in `test_password_hasher.cpp`:
```cpp
#include "domain/PasswordHash.h"
#include <type_traits>
using kinetiq::domain::PasswordHash;

// The point of the type: a password and a hash can't be mixed up silently.
static_assert(!std::is_convertible_v<std::string, PasswordHash>,
              "a plain string must not turn into a PasswordHash by accident");
static_assert(!std::is_convertible_v<PasswordHash, std::string>,
              "a PasswordHash must not be passed where a password is expected");
```
  and change the existing tests to `const PasswordHash hash = hasher.hash(...)`.
- [ ] **Step 2:** Build → fails (`domain/PasswordHash.h` not found).
- [ ] **Step 3: Implement** `PasswordHash.h`:
```cpp
#pragma once
#include <string>
#include <utility>

namespace kinetiq::domain {

// An encoded password hash (argon2's "$argon2id$v=19$..." string). A separate
// type, not a std::string, so the compiler rejects mixing it up with a plain
// password -- e.g. verify(password, hash) with the arguments swapped.
class PasswordHash {
public:
    PasswordHash() = default;
    explicit PasswordHash(std::string encoded) : encoded_(std::move(encoded)) {}

    const std::string& encoded() const { return encoded_; }
    bool operator==(const PasswordHash&) const = default;

private:
    std::string encoded_;
};

}  // namespace kinetiq::domain
```
  `User.h`: `PasswordHash passwordHash;`. Port and Argon2 adapter use the new signatures
  (`return kinetiq::domain::PasswordHash(std::string(...));`, `argon2id_verify(hash.encoded().c_str(), ...)`).
  `PgUserRepository.cpp`: `user.passwordHash = kinetiq::domain::PasswordHash(row["password_hash"].as<std::string>());`
  and `user.passwordHash.encoded()` in both `pqxx::params`.
  Tests (mechanical): `x.passwordHash = "lit";` → `x.passwordHash = kinetiq::domain::PasswordHash("lit");`;
  `EXPECT_EQ(y->passwordHash, "lit")` / `EXPECT_NE(...)` → compare `.encoded()`.
- [ ] **Step 4:** Build + run → all pass. Commit `refactor(auth): PasswordHash strong type`.

### Task 7: `FakePasswordHasher` for service and controller tests

**Files:** Create `Server/tests/support/FakePasswordHasher.h`; modify `test_login_user.cpp`,
`test_register_user.cpp`, `test_change_password.cpp`, `test_auth_controller.cpp`,
`test_user_controller.cpp`, `test_injury_controller.cpp`, `test_catalog_controller.cpp`.

- [ ] **Step 1: Implement the fake** (a test double, so no separate red step; the suites below are its tests):
```cpp
#pragma once
#include "ports/IPasswordHasher.h"
#include <string>

namespace kinetiq::testsupport {

// Hand-written fake for IPasswordHasher: instant and deterministic, so service
// and HTTP tests don't pay argon2's deliberate slowness (~0.3 s, 64 MiB per
// hash). It still never stores the plain password as-is, and it counts calls
// so tests can check how often hashing/verifying happened.
class FakePasswordHasher : public kinetiq::ports::IPasswordHasher {
public:
    kinetiq::domain::PasswordHash hash(const std::string& password) override {
        ++hashCalls;
        return kinetiq::domain::PasswordHash("fake$" + password);
    }
    bool verify(const kinetiq::domain::PasswordHash& hash, const std::string& password) override {
        ++verifyCalls;
        return hash.encoded() == "fake$" + password;
    }

    int hashCalls = 0;
    int verifyCalls = 0;
};

}  // namespace kinetiq::testsupport
```
- [ ] **Step 2:** In the 7 test files replace the include, the `using`, and every
  `Argon2PasswordHasher` with `FakePasswordHasher` (`#include "support/FakePasswordHasher.h"`).
  Update the file comments that say the tests use real crypto.
- [ ] **Step 3:** Build + run → all pass; note the suite time before/after.
- [ ] **Step 4:** Commit `test: FakePasswordHasher replaces argon2 in service/HTTP tests`.

### Task 8: Constant-cost login failures (timing-leak fix)

**Files:** Modify `ports/IPasswordHasher.h`, `adapters/crypto/Argon2PasswordHasher.{h,cpp}`,
`tests/support/FakePasswordHasher.h`, `app/LoginUser.h`, `main.cpp`; tests `test_login_user.cpp`,
`test_password_hasher.cpp`.

**Interfaces:** Produces port method `const domain::PasswordHash& dummyHash()`.

- [ ] **Step 1: Failing tests.** `test_login_user.cpp`:
```cpp
TEST(LoginUserTest, UnknownEmail_StillVerifiesOnce_SoItTakesAsLongAsAWrongPassword) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    EXPECT_THROW(loginUser.execute({"nobody@example.com", "Whatever1"}), InvalidCredentialsError);
    EXPECT_EQ(hasher.verifyCalls, 1);
}

TEST(LoginUserTest, WrongPassword_VerifiesOnce) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);
    seedUser(users, hasher, "erin@example.com", "RightPass1");

    EXPECT_THROW(loginUser.execute({"erin@example.com", "WrongPass1"}), InvalidCredentialsError);
    EXPECT_EQ(hasher.verifyCalls, 1);
}
```
  `test_password_hasher.cpp`:
```cpp
TEST(Argon2PasswordHasherTest, DummyHash_IsARealArgon2HashComputedOnce) {
    Argon2PasswordHasher hasher;
    const PasswordHash& first = hasher.dummyHash();
    EXPECT_EQ(first.encoded().rfind("$argon2id$", 0), 0u);
    EXPECT_EQ(&first, &hasher.dummyHash());  // same object: computed once
    EXPECT_FALSE(hasher.verify(first, "StrongPass1"));
}
```
- [ ] **Step 2:** Build → fails (`dummyHash` missing); unknown-email test would report 0 verify calls.
- [ ] **Step 3: Implement.** Port:
```cpp
// A valid hash of a fixed secret, computed once. LoginUser verifies against it
// when the email is unknown, so "no such user" costs the same time as "wrong
// password" and response timing can't reveal which emails are registered.
virtual const kinetiq::domain::PasswordHash& dummyHash() = 0;
```
  Argon2:
```cpp
const kinetiq::domain::PasswordHash& Argon2PasswordHasher::dummyHash() {
    // Function-local static: computed on first use, thread-safe since C++11.
    // The secret doesn't need to be secret -- it's only ever compared when no
    // real user exists, and the login fails either way.
    static const kinetiq::domain::PasswordHash dummy = hash("kinetiq-timing-equaliser");
    return dummy;
}
```
  Fake: `const PasswordHash& dummyHash() override { return dummy_; }` with
  `kinetiq::domain::PasswordHash dummy_{"fake$<dummy>"};`.
  `LoginUser::execute`:
```cpp
const auto found = users_.findByEmail(request.email);
// Always run exactly one verify (against the dummy hash if there's no such
// user), so both failures take the same time -- see IPasswordHasher::dummyHash.
const bool passwordOk =
    hasher_.verify(found ? found->passwordHash : hasher_.dummyHash(), request.password);
if (!found.has_value() || !passwordOk) {
    throw InvalidCredentialsError();
}
```
  `main.cpp` after creating `hasher`: `hasher.dummyHash();  // compute now, not on the first failed login`.
- [ ] **Step 4:** Build + run → all pass. Commit `fix(auth): unknown-email login costs the same as a wrong password`.

### Task 9: Docs

**Files:** `docs/02-api-contract.md`, `docs/backlog.md`, `docs/reviews/2026-10-26-code-review.md`,
`docs/CONTEXT.md`, `WORKLOG.md`.

- [ ] Contract: password policy "8–64 characters (counted as characters, not bytes), at least one
  uppercase letter"; register / change-password 422 bodies name `password` / `new_password`;
  `PATCH /users/me` limits (`weekly_workout_goal` 0–21, `available_time_minutes` 1–300, real
  calendar `birth_date`) with ✅/❌ test cases.
- [ ] Backlog: profile history should record *who* made a change (user / AI / admin).
- [ ] Review doc: add #26 (history write not atomic with the profile save); mark fixed: #6, #19,
  #21 (number limits), #17 (password cap), timing leak.
- [ ] CONTEXT + WORKLOG entry. Commit `docs: domain review fixes`.
- [ ] Push the branch, open a PR, wait for CI.
