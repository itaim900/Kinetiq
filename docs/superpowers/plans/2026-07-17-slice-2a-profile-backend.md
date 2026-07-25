# Slice 2 · Stage A — Profile Backend (`#19`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An authenticated user can read their full profile (`GET /api/v1/users/me`) and partially update it (`PATCH /api/v1/users/me`), with server-side validation of every provided field.

**Architecture:** Hexagonal, test-first, up the pyramid — exactly as Slice 1. Grow `domain::User` with the profile fields (D1); `PATCH` is load-modify-save in a new `app::UpdateProfile` service (D2); validation lives in a pure `domain::ProfilePolicy` (D7); a new `UserController` exposes the two routes, kept separate from `AuthController` (D6). No migration — the `users` table already has every column.

**Tech Stack:** C++20, Crow (vendored `crow_all.h`), libpqxx, GoogleTest, CMake+Ninja (MSYS2 UCRT64 GCC).

**Spec:** [`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`](../specs/2026-07-17-slice-2-profile-injuries-design.md)

## Global Constraints

- **No new migration** — `0002_core_schema.sql` already defines all `users` profile columns.
- **Units on the wire are metric** (`height_cm`, `weight_kg`); no imperial conversion this stage.
- **`preferences` (jsonb) is out of scope** this slice (D4) — do not read, write, or serialize it.
- **Bounds (D7):** all biometric fields optional (unset ⇒ SQL NULL). Provided values must pass: `weight_kg` in `(0, 500]`, `height_cm` in `(0, 300]`, `weekly_workout_goal` `>= 0`, `available_time_minutes` `> 0`, `experience_level` ∈ {beginner,intermediate,advanced}, `sex` ∈ {male,female,other,unspecified}, `unit_preference` ∈ {metric,imperial}, `birth_date` a plausible `YYYY-MM-DD` (year 1900–2100, month 1–12, day 1–31). Server hard-rejects with `422`.
- **Never serialize `password_hash`.**
- **Build:** from `Server/`, with `C:\msys64\ucrt64\bin` on PATH. Configure/build: `cmake --preset default && cmake --build --preset default`. Tests: `ctest --preset default` (or run the `kinetiq_tests` exe). DB integration tests need Postgres up (`docker compose up -d`); they `GTEST_SKIP()` if not.
- **File map (this stage):**
  - Modify `Server/src/domain/User.h` — profile fields
  - Create `Server/src/domain/ProfilePolicy.h` — `ProfileValidationError` + `requireValidProfile`
  - Create `Server/tests/test_profile_policy.cpp`
  - Modify `Server/src/adapters/db/PgUserRepository.cpp` — map/persist profile columns
  - Modify `Server/tests/test_pg_user_repository.cpp` — profile round-trip (real DB)
  - Modify `Server/tests/test_user_repository.cpp` — profile round-trip (fake)
  - Create `Server/src/app/UpdateProfile.h` — `ProfileUpdate`, `UpdateProfile`, `ProfileUserNotFoundError`
  - Create `Server/tests/test_update_profile.cpp`
  - Create `Server/src/http/dto/ProfileDto.h` — `parseProfileUpdate`, `profileToJson`, `validationErrorResponse`
  - Create `Server/src/http/controllers/UserController.h` / `UserController.cpp` — `registerUserRoutes`
  - Create `Server/tests/test_user_controller.cpp`
  - Modify `Server/src/main.cpp` — call `registerUserRoutes`
  - Modify `Server/CMakeLists.txt` — register new source + test files
  - `Server/tests/support/InMemoryUserRepository.h` — **no change needed** (stores `User` by value; new fields round-trip automatically). Task A2 adds a test that documents this.

---

### Task A1: Profile domain model + validation policy

**Files:**
- Modify: `Server/src/domain/User.h`
- Create: `Server/src/domain/ProfilePolicy.h`
- Test: `Server/tests/test_profile_policy.cpp`
- Modify: `Server/CMakeLists.txt` (add the test source)

**Interfaces:**
- Produces: `domain::User` with profile fields (below); `domain::ProfileValidationError` (has `field()`); `domain::requireValidProfile(const domain::User&)` (throws `ProfileValidationError` on the first bad field, else returns void).

- [ ] **Step 1: Grow `domain::User`**

Replace `Server/src/domain/User.h` with:

```cpp
#pragma once

#include <optional>
#include <string>

namespace kinetiq::domain {

// The user aggregate. Slice 1 needed only the auth subset; Slice 2 adds the
// profile fields. All profile fields are optional (an unset field is NULL in
// the DB -- the user is never forced to provide biometrics), except
// unitPreference, which the schema stores NOT NULL DEFAULT 'metric'.
struct User {
    long long id = 0;
    std::string email;
    std::string passwordHash;
    std::string displayName;

    // --- profile (Slice 2) ---
    std::optional<std::string> sex;              // male|female|other|unspecified
    std::optional<std::string> birthDate;        // ISO YYYY-MM-DD (age derived at read time)
    std::optional<double> heightCm;
    std::optional<double> weightKg;
    std::optional<std::string> experienceLevel;  // beginner|intermediate|advanced
    std::string unitPreference = "metric";       // metric|imperial (NOT NULL in DB)
    std::optional<int> weeklyWorkoutGoal;
    std::optional<int> availableTimeMinutes;
    std::optional<std::string> medications;      // free text in v1
};

}  // namespace kinetiq::domain
```

- [ ] **Step 2: Write the failing test**

Create `Server/tests/test_profile_policy.cpp`:

```cpp
// Slice 2 (#19): ProfilePolicy -- pure, DB-free validation of profile fields,
// mirroring PasswordPolicy. requireValidProfile throws ProfileValidationError
// (carrying the offending field name) on the first invalid field.

#include <gtest/gtest.h>

#include "domain/ProfilePolicy.h"
#include "domain/User.h"

using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::requireValidProfile;
using kinetiq::domain::User;

namespace {

// A fully-valid profile; individual tests corrupt one field.
User validProfile() {
    User u;
    u.email = "v@example.com";
    u.passwordHash = "x";
    u.sex = "female";
    u.birthDate = "1990-05-15";
    u.heightCm = 170.0;
    u.weightKg = 65.0;
    u.experienceLevel = "intermediate";
    u.unitPreference = "metric";
    u.weeklyWorkoutGoal = 4;
    u.availableTimeMinutes = 45;
    return u;
}

}  // namespace

TEST(ProfilePolicy, FullyValidProfile_DoesNotThrow) {
    EXPECT_NO_THROW(requireValidProfile(validProfile()));
}

TEST(ProfilePolicy, EmptyProfile_DoesNotThrow) {
    // All optionals unset, unitPreference at its default -- nothing to reject.
    User u;
    u.email = "e@example.com";
    u.passwordHash = "x";
    EXPECT_NO_THROW(requireValidProfile(u));
}

TEST(ProfilePolicy, ZeroWeight_Throws_FieldIsWeightKg) {
    User u = validProfile();
    u.weightKg = 0.0;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "weight_kg");
    }
}

TEST(ProfilePolicy, NegativeWeight_Throws) {
    User u = validProfile();
    u.weightKg = -1.0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ImplausiblyHighWeight_Throws) {
    User u = validProfile();
    u.weightKg = 501.0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ImplausiblyHighHeight_Throws_FieldIsHeightCm) {
    User u = validProfile();
    u.heightCm = 300.1;
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "height_cm");
    }
}

TEST(ProfilePolicy, BadExperienceLevel_Throws) {
    User u = validProfile();
    u.experienceLevel = "expert";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, BadSex_Throws) {
    User u = validProfile();
    u.sex = "yes";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, BadUnitPreference_Throws) {
    User u = validProfile();
    u.unitPreference = "furlongs";
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, NegativeWeeklyGoal_Throws) {
    User u = validProfile();
    u.weeklyWorkoutGoal = -1;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, ZeroAvailableTime_Throws) {
    User u = validProfile();
    u.availableTimeMinutes = 0;
    EXPECT_THROW(requireValidProfile(u), ProfileValidationError);
}

TEST(ProfilePolicy, MalformedBirthDate_Throws_FieldIsBirthDate) {
    User u = validProfile();
    u.birthDate = "May 5th 1990";
    try {
        requireValidProfile(u);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "birth_date");
    }
}
```

- [ ] **Step 3: Register the test in CMake, then verify it fails to compile**

In `Server/CMakeLists.txt`, inside the `add_executable(kinetiq_tests ...)` list, add after `tests/test_password_policy.cpp`:

```cmake
        tests/test_profile_policy.cpp
```

Run (from `Server/`):

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — `fatal error: domain/ProfilePolicy.h: No such file or directory`.

- [ ] **Step 4: Write `ProfilePolicy.h`**

Create `Server/src/domain/ProfilePolicy.h`:

```cpp
#pragma once

#include "domain/User.h"

#include <cstdio>
#include <stdexcept>
#include <string>

namespace kinetiq::domain {

// Thrown when a profile field is invalid. Carries the offending field's
// wire name (e.g. "weight_kg") so the HTTP layer can build the RFC 9457
// `errors: { field: [...] }` object.
class ProfileValidationError : public std::runtime_error {
public:
    ProfileValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

namespace detail {

inline bool isOneOf(const std::string& v, std::initializer_list<const char*> allowed) {
    for (const char* a : allowed) {
        if (v == a) return true;
    }
    return false;
}

// Plausible YYYY-MM-DD: correct shape and sane calendar ranges. Deliberately
// does NOT check "not in the future" against the wall clock -- that would make
// the validator (and its tests) time-dependent; the 2100 ceiling is enough for
// v1. A strict not-future check is a documented deferral.
inline bool isPlausibleBirthDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    int y = 0, m = 0, d = 0;
    if (std::sscanf(s.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) return false;
    return y >= 1900 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

}  // namespace detail

// Validates every provided profile field on `u`. Throws ProfileValidationError
// on the first invalid field; returns normally if all present fields are valid.
// Unset optionals are skipped (an absent field is always valid).
inline void requireValidProfile(const User& u) {
    if (u.weightKg && (*u.weightKg <= 0.0 || *u.weightKg > 500.0)) {
        throw ProfileValidationError("weight_kg", "weight_kg must be between 0 and 500");
    }
    if (u.heightCm && (*u.heightCm <= 0.0 || *u.heightCm > 300.0)) {
        throw ProfileValidationError("height_cm", "height_cm must be between 0 and 300");
    }
    if (u.experienceLevel &&
        !detail::isOneOf(*u.experienceLevel, {"beginner", "intermediate", "advanced"})) {
        throw ProfileValidationError("experience_level",
                                     "experience_level must be beginner, intermediate, or advanced");
    }
    if (u.sex && !detail::isOneOf(*u.sex, {"male", "female", "other", "unspecified"})) {
        throw ProfileValidationError("sex", "sex must be male, female, other, or unspecified");
    }
    if (!detail::isOneOf(u.unitPreference, {"metric", "imperial"})) {
        throw ProfileValidationError("unit_preference", "unit_preference must be metric or imperial");
    }
    if (u.weeklyWorkoutGoal && *u.weeklyWorkoutGoal < 0) {
        throw ProfileValidationError("weekly_workout_goal", "weekly_workout_goal must be >= 0");
    }
    if (u.availableTimeMinutes && *u.availableTimeMinutes <= 0) {
        throw ProfileValidationError("available_time_minutes", "available_time_minutes must be > 0");
    }
    if (u.birthDate && !detail::isPlausibleBirthDate(*u.birthDate)) {
        throw ProfileValidationError("birth_date", "birth_date must be a valid YYYY-MM-DD date");
    }
}

}  // namespace kinetiq::domain
```

- [ ] **Step 5: Build and run the test — verify green**

Run:

```
cmake --build --preset default --target kinetiq_tests
ctest --preset default -R ProfilePolicy --output-on-failure
```

Expected: **PASS** — 12/12 `ProfilePolicy.*` tests.

- [ ] **Step 6: Commit**

```
git add Server/src/domain/User.h Server/src/domain/ProfilePolicy.h Server/tests/test_profile_policy.cpp Server/CMakeLists.txt
git commit -m "feat(server): profile domain fields + ProfilePolicy validation (TDD) #19"
```

---

### Task A2: Persist profile columns (PgUserRepository + fake round-trip)

**Files:**
- Modify: `Server/src/adapters/db/PgUserRepository.cpp`
- Modify: `Server/tests/test_pg_user_repository.cpp` (real DB round-trip)
- Modify: `Server/tests/test_user_repository.cpp` (fake round-trip)

**Interfaces:**
- Consumes: `domain::User` (Task A1). `IUserRepository::{findById,update}` signatures are unchanged — only the SQL/mapping grows.
- Produces: `findById`/`findByEmail` now populate profile fields; `update()` now persists them.

- [ ] **Step 1: Write the failing DB test**

In `Server/tests/test_pg_user_repository.cpp`, append before the final closing (end of file):

```cpp
// Slice 2 (#19): profile columns must round-trip through update() + findById().
TEST_F(DbTest, PgUserRepository_Update_PersistsProfileFields) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "pg-profile@example.com";
    user.passwordHash = "hashed";
    const User saved = repo.save(user);

    User modified = saved;
    modified.sex = "female";
    modified.birthDate = "1988-03-09";
    modified.heightCm = 168.5;
    modified.weightKg = 61.25;
    modified.experienceLevel = "advanced";
    modified.unitPreference = "imperial";
    modified.weeklyWorkoutGoal = 5;
    modified.availableTimeMinutes = 60;
    modified.medications = "none";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->sex, "female");
    EXPECT_EQ(found->birthDate, "1988-03-09");
    ASSERT_TRUE(found->heightCm.has_value());
    EXPECT_DOUBLE_EQ(*found->heightCm, 168.5);
    ASSERT_TRUE(found->weightKg.has_value());
    EXPECT_DOUBLE_EQ(*found->weightKg, 61.25);
    EXPECT_EQ(found->experienceLevel, "advanced");
    EXPECT_EQ(found->unitPreference, "imperial");
    EXPECT_EQ(found->weeklyWorkoutGoal, 5);
    EXPECT_EQ(found->availableTimeMinutes, 60);
    EXPECT_EQ(found->medications, "none");
}

// A freshly-saved user has NULL profile columns -> all optionals empty,
// unitPreference at the DB default 'metric'.
TEST_F(DbTest, PgUserRepository_FindById_NewUserHasEmptyProfile) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "pg-empty-profile@example.com";
    user.passwordHash = "hashed";
    const User saved = repo.save(user);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->sex.has_value());
    EXPECT_FALSE(found->weightKg.has_value());
    EXPECT_EQ(found->unitPreference, "metric");
}
```

- [ ] **Step 2: Run the DB test — verify it fails**

Ensure Postgres is up (`docker compose up -d` from repo root), then:

```
cmake --build --preset default --target kinetiq_tests
ctest --preset default -R PgUserRepository_Update_PersistsProfileFields --output-on-failure
```

Expected: **FAIL** — the assertions on `found->sex` etc. fail because `update()`/`rowToUser` don't handle the new columns yet (fields come back empty). (If Postgres is down the test SKIPs — start it first so you see a real red.)

- [ ] **Step 3: Extend `PgUserRepository.cpp`**

Replace the anonymous-namespace `rowToUser` and the `findByEmail`/`findById`/`update` bodies in `Server/src/adapters/db/PgUserRepository.cpp` so they read/write the profile columns. `save()` is unchanged.

Replace `rowToUser`:

```cpp
kinetiq::domain::User rowToUser(const pqxx::row& row) {
    kinetiq::domain::User user;
    user.id = row["id"].as<long long>();
    user.email = row["email"].as<std::string>();
    user.passwordHash = row["password_hash"].as<std::string>();
    user.displayName = row["display_name"].is_null() ? "" : row["display_name"].as<std::string>();

    if (!row["sex"].is_null()) user.sex = row["sex"].as<std::string>();
    if (!row["birth_date"].is_null()) user.birthDate = row["birth_date"].as<std::string>();
    if (!row["height_cm"].is_null()) user.heightCm = row["height_cm"].as<double>();
    if (!row["weight_kg"].is_null()) user.weightKg = row["weight_kg"].as<double>();
    if (!row["experience_level"].is_null())
        user.experienceLevel = row["experience_level"].as<std::string>();
    user.unitPreference =
        row["unit_preference"].is_null() ? "metric" : row["unit_preference"].as<std::string>();
    if (!row["weekly_workout_goal"].is_null())
        user.weeklyWorkoutGoal = row["weekly_workout_goal"].as<int>();
    if (!row["available_time_minutes"].is_null())
        user.availableTimeMinutes = row["available_time_minutes"].as<int>();
    if (!row["medications"].is_null()) user.medications = row["medications"].as<std::string>();
    return user;
}
```

Replace the two `SELECT` statements (`findByEmail` and `findById`) to select the full column list. Use this exact column list in both (only the `WHERE` differs):

```cpp
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name, sex, birth_date, height_cm, weight_kg, "
        "experience_level, unit_preference, weekly_workout_goal, available_time_minutes, "
        "medications FROM users WHERE email = $1",
        pqxx::params{email});
```

```cpp
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name, sex, birth_date, height_cm, weight_kg, "
        "experience_level, unit_preference, weekly_workout_goal, available_time_minutes, "
        "medications FROM users WHERE id = $1",
        pqxx::params{id});
```

Replace `update()`:

```cpp
void PgUserRepository::update(const kinetiq::domain::User& user) {
    try {
        tx_.exec(
            "UPDATE users SET email = $1, password_hash = $2, display_name = $3, sex = $4, "
            "birth_date = $5, height_cm = $6, weight_kg = $7, experience_level = $8, "
            "unit_preference = $9, weekly_workout_goal = $10, available_time_minutes = $11, "
            "medications = $12 WHERE id = $13",
            pqxx::params{user.email, user.passwordHash, user.displayName, user.sex, user.birthDate,
                         user.heightCm, user.weightKg, user.experienceLevel, user.unitPreference,
                         user.weeklyWorkoutGoal, user.availableTimeMinutes, user.medications,
                         user.id});
    } catch (const pqxx::unique_violation&) {
        throw kinetiq::ports::DuplicateEmailError(user.email);
    }
}
```

Note: `pqxx::params` maps an empty `std::optional<T>` to SQL `NULL` and a populated one to its value; the `birth_date` string is cast to `DATE` by Postgres.

- [ ] **Step 4: Add the fake round-trip test**

In `Server/tests/test_user_repository.cpp`, append a test proving the in-memory fake also round-trips the new fields (it does automatically — this documents and guards it):

```cpp
// Slice 2 (#19): the fake stores a whole User by value, so profile fields
// round-trip through update()/findById() with no fake changes -- guard it.
TEST(InMemoryUserRepository, Update_PersistsProfileFields) {
    kinetiq::testsupport::InMemoryUserRepository repo;
    kinetiq::domain::User user;
    user.email = "mem-profile@example.com";
    user.passwordHash = "x";
    const auto saved = repo.save(user);

    auto modified = saved;
    modified.weightKg = 70.0;
    modified.experienceLevel = "beginner";
    modified.unitPreference = "imperial";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->weightKg, 70.0);
    EXPECT_EQ(found->experienceLevel, "beginner");
    EXPECT_EQ(found->unitPreference, "imperial");
}
```

Check `test_user_repository.cpp`'s existing includes cover `domain/User.h`, `support/InMemoryUserRepository.h`, and `<gtest/gtest.h>`; add any that are missing.

- [ ] **Step 5: Build and run — verify green**

```
cmake --build --preset default --target kinetiq_tests
ctest --preset default -R "PgUserRepository|InMemoryUserRepository" --output-on-failure
```

Expected: **PASS** — the two new DB tests + the new fake test green, and all previously-passing repo tests still green (Slice 1's `update()` change-password test still works because it loads a full `User` first).

- [ ] **Step 6: Commit**

```
git add Server/src/adapters/db/PgUserRepository.cpp Server/tests/test_pg_user_repository.cpp Server/tests/test_user_repository.cpp
git commit -m "feat(server): persist profile columns via PgUserRepository (TDD) #19"
```

---

### Task A3: `UpdateProfile` application service

**Files:**
- Create: `Server/src/app/UpdateProfile.h`
- Test: `Server/tests/test_update_profile.cpp`
- Modify: `Server/CMakeLists.txt` (add the test source)

**Interfaces:**
- Consumes: `ports::IUserRepository` (`findById`, `update`); `domain::requireValidProfile` (A1).
- Produces:
  - `struct app::ProfileUpdate { std::optional<std::string> sex, birthDate, experienceLevel, unitPreference, medications; std::optional<double> heightCm, weightKg; std::optional<int> weeklyWorkoutGoal, availableTimeMinutes; };`
  - `class app::UpdateProfile { explicit UpdateProfile(ports::IUserRepository&); domain::User execute(long long userId, const ProfileUpdate&); };`
  - `class app::ProfileUserNotFoundError : public std::runtime_error;`

- [ ] **Step 1: Write the failing test**

Create `Server/tests/test_update_profile.cpp`:

```cpp
// Slice 2 (#19): UpdateProfile -- load-modify-save. Applies only the present
// fields of a ProfileUpdate onto the current user, validates the result, then
// persists. Tested against the in-memory fake.

#include <gtest/gtest.h>

#include "app/UpdateProfile.h"
#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::app::ProfileUpdate;
using kinetiq::app::ProfileUserNotFoundError;
using kinetiq::app::UpdateProfile;
using kinetiq::domain::ProfileValidationError;
using kinetiq::domain::User;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

// Seed a user with a known display name + one preset field, return its id.
long long seedUser(InMemoryUserRepository& repo) {
    User u;
    u.email = "seed@example.com";
    u.passwordHash = "x";
    u.displayName = "Seed";
    u.experienceLevel = "beginner";
    return repo.save(u).id;
}

}  // namespace

TEST(UpdateProfile, PartialUpdate_ChangesOnlyProvidedFields) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.weightKg = 72.5;  // only weight provided

    UpdateProfile useCase(repo);
    const User result = useCase.execute(id, update);

    EXPECT_EQ(result.weightKg, 72.5);
    EXPECT_EQ(result.experienceLevel, "beginner");  // untouched
    EXPECT_EQ(result.displayName, "Seed");          // untouched

    const auto persisted = repo.findById(id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_EQ(persisted->weightKg, 72.5);
    EXPECT_EQ(persisted->experienceLevel, "beginner");
}

TEST(UpdateProfile, InvalidValue_Throws_AndPersistsNothing) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.weightKg = -5.0;  // invalid

    UpdateProfile useCase(repo);
    EXPECT_THROW(useCase.execute(id, update), ProfileValidationError);

    const auto persisted = repo.findById(id);
    ASSERT_TRUE(persisted.has_value());
    EXPECT_FALSE(persisted->weightKg.has_value());  // unchanged
}

TEST(UpdateProfile, UnknownUser_ThrowsProfileUserNotFound) {
    InMemoryUserRepository repo;
    UpdateProfile useCase(repo);
    ProfileUpdate update;
    update.weightKg = 70.0;
    EXPECT_THROW(useCase.execute(999999, update), ProfileUserNotFoundError);
}
```

- [ ] **Step 2: Register the test in CMake, verify red**

In `Server/CMakeLists.txt`, in the `kinetiq_tests` list, add after `tests/test_change_password.cpp`:

```cmake
        tests/test_update_profile.cpp
```

Run:

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — `fatal error: app/UpdateProfile.h: No such file or directory`.

- [ ] **Step 3: Write `UpdateProfile.h`**

Create `Server/src/app/UpdateProfile.h`:

```cpp
#pragma once

#include "domain/ProfilePolicy.h"
#include "domain/User.h"
#include "ports/IUserRepository.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace kinetiq::app {

// Thrown when the authenticated user's id resolves to no user (defensive --
// shouldn't happen for a valid token, but handled rather than dereferencing an
// empty optional). The HTTP layer maps this to 401, same as GET /auth/me does
// for a valid-but-nonexistent user.
class ProfileUserNotFoundError : public std::runtime_error {
public:
    ProfileUserNotFoundError() : std::runtime_error("authenticated user not found") {}
};

// A partial profile update: each present field is applied, each absent field is
// left unchanged (D3 -- no null-clearing in v1).
struct ProfileUpdate {
    std::optional<std::string> sex;
    std::optional<std::string> birthDate;
    std::optional<double> heightCm;
    std::optional<double> weightKg;
    std::optional<std::string> experienceLevel;
    std::optional<std::string> unitPreference;
    std::optional<int> weeklyWorkoutGoal;
    std::optional<int> availableTimeMinutes;
    std::optional<std::string> medications;
};

// Use case: load the current user, apply the provided fields, validate the
// resulting profile, persist. Depends only on the repository port.
class UpdateProfile {
public:
    explicit UpdateProfile(kinetiq::ports::IUserRepository& users) : users_(users) {}

    kinetiq::domain::User execute(long long userId, const ProfileUpdate& update) {
        auto found = users_.findById(userId);
        if (!found.has_value()) {
            throw ProfileUserNotFoundError();
        }

        kinetiq::domain::User user = *found;
        if (update.sex) user.sex = update.sex;
        if (update.birthDate) user.birthDate = update.birthDate;
        if (update.heightCm) user.heightCm = update.heightCm;
        if (update.weightKg) user.weightKg = update.weightKg;
        if (update.experienceLevel) user.experienceLevel = update.experienceLevel;
        if (update.unitPreference) user.unitPreference = *update.unitPreference;
        if (update.weeklyWorkoutGoal) user.weeklyWorkoutGoal = update.weeklyWorkoutGoal;
        if (update.availableTimeMinutes) user.availableTimeMinutes = update.availableTimeMinutes;
        if (update.medications) user.medications = update.medications;

        // Guard: reject an invalid resulting profile BEFORE persisting.
        kinetiq::domain::requireValidProfile(user);

        users_.update(user);
        return user;
    }

private:
    kinetiq::ports::IUserRepository& users_;
};

}  // namespace kinetiq::app
```

- [ ] **Step 4: Build and run — verify green**

```
cmake --build --preset default --target kinetiq_tests
ctest --preset default -R UpdateProfile --output-on-failure
```

Expected: **PASS** — 3/3 `UpdateProfile.*`.

- [ ] **Step 5: Commit**

```
git add Server/src/app/UpdateProfile.h Server/tests/test_update_profile.cpp Server/CMakeLists.txt
git commit -m "feat(server): UpdateProfile service, load-modify-save (TDD) #19"
```

---

### Task A4: HTTP layer — `ProfileDto` + `UserController` (`GET`/`PATCH /users/me`)

**Files:**
- Create: `Server/src/http/dto/ProfileDto.h`
- Create: `Server/src/http/controllers/UserController.h`
- Create: `Server/src/http/controllers/UserController.cpp`
- Test: `Server/tests/test_user_controller.cpp`
- Modify: `Server/CMakeLists.txt` (add `UserController.cpp` to **both** `kinetiq_server` and `kinetiq_tests`; add the test source)

**Interfaces:**
- Consumes: `app::UpdateProfile`/`ProfileUpdate`/`ProfileUserNotFoundError` (A3); `domain::ProfileValidationError` (A1); `http::authenticate` + `CurrentUser` (Slice 1, `http/middleware/Authenticator.h`); `dto::errorResponse` (Slice 1, `http/dto/AuthDto.h`).
- Produces: `dto::parseProfileUpdate(const crow::json::rvalue&) -> app::ProfileUpdate`; `dto::profileToJson(const domain::User&) -> crow::json::wvalue`; `dto::validationErrorResponse(field, message) -> crow::response`; `http::registerUserRoutes(crow::SimpleApp&, ports::IUserRepository&, ports::ITokenService&)`.

- [ ] **Step 1: Write the failing HTTP test**

Create `Server/tests/test_user_controller.cpp`:

```cpp
// Slice 2 (#19): HTTP layer for GET/PATCH /users/me. Drives real routing/
// handlers through HttpClient (app.handle_full(), no socket). Registers the
// auth routes too, so tests obtain a real token via register.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/UserController.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

// Wire an app with auth + user routes over one shared fake repo.
struct UserApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    UserApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerUserRoutes(app, users, tokens);
    }
};

template <typename App>
std::string registerAndGetToken(kinetiq::testing::HttpClient<App>& client,
                                const std::string& email) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":")" + email + R"(","password":"StrongPass1","display_name":"Test"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(UserControllerTest, GetMe_ValidToken_Returns200WithProfile) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "gm@example.com");

    const auto r = client.get("/api/v1/users/me", {{"Authorization", "Bearer " + token}});

    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["email"].s()), "gm@example.com");
    EXPECT_EQ(std::string(body["unit_preference"].s()), "metric");  // DB default
    EXPECT_FALSE(body.has("password_hash"));
}

TEST(UserControllerTest, GetMe_MissingToken_Returns401) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    EXPECT_EQ(client.get("/api/v1/users/me").code, 401);
}

TEST(UserControllerTest, PatchMe_PartialUpdate_Returns200AndPersists) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "pm@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"weight_kg":74.5,"experience_level":"intermediate"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);

    const auto after =
        crow::json::load(client.get("/api/v1/users/me", {{"Authorization", "Bearer " + token}}).body);
    ASSERT_TRUE(after);
    EXPECT_DOUBLE_EQ(after["weight_kg"].d(), 74.5);
    EXPECT_EQ(std::string(after["experience_level"].s()), "intermediate");
}

TEST(UserControllerTest, PatchMe_InvalidWeight_Returns422WithFieldError) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "iw@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"weight_kg":-3})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_TRUE(body["errors"].has("weight_kg"));
}

TEST(UserControllerTest, PatchMe_BadExperienceLevel_Returns422) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "be@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"experience_level":"expert"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(UserControllerTest, PatchMe_MissingToken_Returns401) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":70})");
    EXPECT_EQ(r.code, 401);
}
```

- [ ] **Step 2: Register test + controller in CMake, verify red**

In `Server/CMakeLists.txt`:
- Add to the `kinetiq_server` `add_executable` source list, after `src/http/controllers/AuthController.cpp`:
  ```cmake
      src/http/controllers/UserController.cpp
  ```
- Add to the `kinetiq_tests` `add_executable` source list, after `tests/test_auth_controller.cpp`:
  ```cmake
        tests/test_user_controller.cpp
  ```
- Add to the `kinetiq_tests` list's source-file section, after its `src/http/controllers/AuthController.cpp` line:
  ```cmake
        src/http/controllers/UserController.cpp
  ```

Run:

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — `UserController.cpp` / `http/controllers/UserController.h` / `http/dto/ProfileDto.h` not found (CMake/compile-level red).

- [ ] **Step 3: Write `ProfileDto.h`**

Create `Server/src/http/dto/ProfileDto.h`:

```cpp
#pragma once

#include "app/UpdateProfile.h"
#include "domain/User.h"

#include "crow_all.h"

#include <string>

namespace kinetiq::http::dto {

// Parses a PATCH /users/me body into a ProfileUpdate. Only keys actually
// present are set -- an absent key leaves that field unchanged (D3).
inline kinetiq::app::ProfileUpdate parseProfileUpdate(const crow::json::rvalue& body) {
    kinetiq::app::ProfileUpdate u;
    if (body.has("sex")) u.sex = std::string(body["sex"].s());
    if (body.has("birth_date")) u.birthDate = std::string(body["birth_date"].s());
    if (body.has("height_cm")) u.heightCm = body["height_cm"].d();
    if (body.has("weight_kg")) u.weightKg = body["weight_kg"].d();
    if (body.has("experience_level")) u.experienceLevel = std::string(body["experience_level"].s());
    if (body.has("unit_preference")) u.unitPreference = std::string(body["unit_preference"].s());
    if (body.has("weekly_workout_goal"))
        u.weeklyWorkoutGoal = static_cast<int>(body["weekly_workout_goal"].i());
    if (body.has("available_time_minutes"))
        u.availableTimeMinutes = static_cast<int>(body["available_time_minutes"].i());
    if (body.has("medications")) u.medications = std::string(body["medications"].s());
    return u;
}

namespace detail {
// crow::json::wvalue for an optional: the value, or JSON null when unset.
template <typename T>
inline crow::json::wvalue orNull(const std::optional<T>& opt) {
    return opt ? crow::json::wvalue(*opt) : crow::json::wvalue(nullptr);
}
}  // namespace detail

// Full profile serialization. Unset fields serialize as JSON null so the
// client can bind every form field. Never emits password_hash. Excludes
// `preferences` (out of scope this slice, D4).
inline crow::json::wvalue profileToJson(const kinetiq::domain::User& user) {
    crow::json::wvalue json;
    json["id"] = user.id;
    json["email"] = user.email;
    json["display_name"] = user.displayName;
    json["sex"] = detail::orNull(user.sex);
    json["birth_date"] = detail::orNull(user.birthDate);
    json["height_cm"] = detail::orNull(user.heightCm);
    json["weight_kg"] = detail::orNull(user.weightKg);
    json["experience_level"] = detail::orNull(user.experienceLevel);
    json["unit_preference"] = user.unitPreference;
    json["weekly_workout_goal"] = detail::orNull(user.weeklyWorkoutGoal);
    json["available_time_minutes"] = detail::orNull(user.availableTimeMinutes);
    json["medications"] = detail::orNull(user.medications);
    return json;
}

// RFC 9457 validation error with a per-field `errors` map (contract shape).
inline crow::response validationErrorResponse(const std::string& field,
                                              const std::string& message) {
    crow::json::wvalue json;
    json["type"] = "validation_error";
    json["title"] = "Validation Failed";
    json["status"] = 422;
    json["detail"] = "One or more fields are invalid.";
    json["errors"][field][0] = message;
    return crow::response(422, json);
}

}  // namespace kinetiq::http::dto
```

- [ ] **Step 4: Write `UserController.h`**

Create `Server/src/http/controllers/UserController.h`:

```cpp
#pragma once

#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers GET/PATCH /api/v1/users/me on `app`. A free function (matches
// AuthController's pattern) so routes stay testable via HttpClient. Kept
// separate from AuthController: /auth/* is authentication, /users/* is profile.
void registerUserRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                        kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
```

- [ ] **Step 5: Write `UserController.cpp`**

Create `Server/src/http/controllers/UserController.cpp`. Both methods live on one route (Crow handles multiple methods per rule cleanly, and it lets the shared auth check sit in one place):

```cpp
#include "http/controllers/UserController.h"

#include "app/UpdateProfile.h"
#include "domain/ProfilePolicy.h"
#include "http/dto/AuthDto.h"      // dto::errorResponse (shared RFC 9457 helper)
#include "http/dto/ProfileDto.h"
#include "http/middleware/Authenticator.h"

namespace kinetiq::http {

void registerUserRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
                        kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/users/me")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Patch)(
            [&users, &tokens](const crow::request& req) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }

                if (req.method == crow::HTTPMethod::Get) {
                    const auto found = users.findById(currentUser->userId);
                    if (!found.has_value()) {
                        return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                                  "Missing, invalid, or expired token.");
                    }
                    return crow::response(200, dto::profileToJson(*found));
                }

                // PATCH
                const auto body = crow::json::load(req.body);
                if (!body) {
                    return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                              "Request body is not valid JSON.");
                }

                kinetiq::app::UpdateProfile updateProfile(users);
                try {
                    const auto updated =
                        updateProfile.execute(currentUser->userId, dto::parseProfileUpdate(body));
                    return crow::response(200, dto::profileToJson(updated));
                } catch (const kinetiq::domain::ProfileValidationError& e) {
                    // Specific catch first: derives from std::runtime_error.
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const kinetiq::app::ProfileUserNotFoundError&) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                } catch (const std::exception&) {
                    // Wrong-typed JSON fields land here (e.g. weight as a string).
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });
}

}  // namespace kinetiq::http
```

- [ ] **Step 6: Build and run — verify green**

```
cmake --build --preset default --target kinetiq_tests
ctest --preset default -R UserControllerTest --output-on-failure
```

Expected: **PASS** — 6/6 `UserControllerTest.*`.

- [ ] **Step 7: Full suite regression check**

```
cmake --build --preset default
ctest --preset default --output-on-failure
```

Expected: all previously-green tests still pass, plus the new Profile/UpdateProfile/UserController tests. (DB tests SKIP if Postgres is down — start it for a full run.)

- [ ] **Step 8: Commit**

```
git add Server/src/http/dto/ProfileDto.h Server/src/http/controllers/UserController.h Server/src/http/controllers/UserController.cpp Server/tests/test_user_controller.cpp Server/CMakeLists.txt
git commit -m "feat(server): GET/PATCH /users/me HTTP layer (TDD) #19"
```

---

### Task A5: Wire into `main.cpp` + live-verify

**Files:**
- Modify: `Server/src/main.cpp`

**Interfaces:**
- Consumes: `http::registerUserRoutes` (A4); the existing `users` (`AutoCommitPgUserRepository`) and `tokens` (`JwtTokenService`) locals already in `main()`.

- [ ] **Step 1: Add the include**

In `Server/src/main.cpp`, after `#include "http/controllers/AuthController.h"`:

```cpp
#include "http/controllers/UserController.h"
```

- [ ] **Step 2: Register the routes**

In `main.cpp`, immediately after the existing `kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);` line, add:

```cpp
    kinetiq::http::registerUserRoutes(app, users, tokens);
```

- [ ] **Step 3: Build the server**

```
cmake --build --preset default --target kinetiq_server
```

Expected: clean build.

- [ ] **Step 4: Live-verify against real server + Postgres**

Start Postgres (`docker compose up -d` from repo root), then run the server via the wrapper (from repo root):

```
./scripts/run-dev.ps1 -Target server
```

In a second shell, exercise the flow (replace `$TOKEN` with the `access_token` from register):

```
# register -> capture access_token
curl -s -X POST localhost:8080/api/v1/auth/register -H "Content-Type: application/json" \
  -d '{"email":"prof@example.com","password":"StrongPass1","display_name":"Prof"}'

# GET profile (expect 200, unit_preference "metric", null biometrics)
curl -s localhost:8080/api/v1/users/me -H "Authorization: Bearer $TOKEN"

# PATCH partial (expect 200, weight + experience updated)
curl -s -X PATCH localhost:8080/api/v1/users/me -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{"weight_kg":74,"experience_level":"intermediate"}'

# GET again (expect weight_kg 74, experience_level "intermediate")
curl -s localhost:8080/api/v1/users/me -H "Authorization: Bearer $TOKEN"

# invalid PATCH (expect 422 with errors.weight_kg)
curl -s -o /dev/null -w "%{http_code}\n" -X PATCH localhost:8080/api/v1/users/me \
  -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" -d '{"weight_kg":-2}'

# no-token GET (expect 401)
curl -s -o /dev/null -w "%{http_code}\n" localhost:8080/api/v1/users/me
```

Confirm each response matches the expectation. Then clean up the test user from the dev DB and stop the server.

- [ ] **Step 5: Commit**

```
git add Server/src/main.cpp
git commit -m "feat(server): wire /users/me routes into main; live-verified #19"
```

---

## Stage A done — definition of done

- `GET`/`PATCH /api/v1/users/me` implemented, `ctest` fully green (DB tests pass with Postgres up), live-verified via curl.
- **Next:** Stage B (`#20` injuries) gets its own plan (`docs/superpowers/plans/2026-07-17-slice-2b-injuries-backend.md`), then Stage C (`#25` frontend). Update `WORKLOG.md` / `CONTEXT.md` and start the living `docs/phase-2-slice-2-profile-injuries.md` build log as Stage A lands (mirrors Slice 1's doc rhythm).

---

## Self-review notes (author check against the spec)

- **Spec coverage:** `#19` `GET`/`PATCH /users/me` ✓ (A4/A5); domain expansion D1 ✓ (A1); load-modify-save D2 ✓ (A3); no null-clearing D3 ✓ (`ProfileUpdate` optionals, absent = unchanged); `preferences` deferred D4 ✓ (never touched); distinct `/auth/me` D6 ✓ (separate `UserController`, `/auth/me` untouched); domain validation D7 ✓ (A1); RFC 9457 field errors D8 ✓ (`validationErrorResponse`). Injuries (`#20`) and frontend (`#25`) are intentionally out of this stage's plan.
- **Deferral honored:** birth_date "not in the future" strict check deferred (calendar-sanity only) to keep the validator clock-independent — noted in `ProfilePolicy.h`.
- **Type consistency:** `ProfileUpdate` field names/types identical across A3 (definition), A4 (`parseProfileUpdate`), and the `UpdateProfile::execute` application. `requireValidProfile`, `ProfileValidationError::field()`, `ProfileUserNotFoundError`, `registerUserRoutes` signatures consistent across tasks.
- **Watch item:** if Crow rejects a single rule bound to both `Get` and `Patch` at `app.validate()` time (it shouldn't), fall back to two separate `CROW_ROUTE` registrations for the same path with one method each.
