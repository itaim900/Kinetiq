# Slice 2 · Stage B — Injuries Backend (`#20`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** An authenticated user can create, list, edit, and delete their own injuries via `/api/v1/users/me/injuries`, with another user's injuries invisible (cross-user access returns 404).

**Architecture:** A full new vertical stack mirroring the Slice-1 user stack: `domain::Injury` + validation → `IInjuryRepository` port + in-memory fake → `PgInjuryRepository` (+ `AutoCommit` production wrapper) → one cohesive `app::InjuryService` (ownership check returns 404) → `InjuryDto` + `InjuryController` → wired in `main.cpp`. No migration — `user_injuries` already exists (Slice 0).

**Tech Stack:** C++20, Crow (vendored `crow_all.h`), libpqxx, GoogleTest, CMake+Ninja (MSYS2 UCRT64 GCC).

**Spec:** [`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`](../specs/2026-07-17-slice-2-profile-injuries-design.md) (D5 = one `InjuryService`; ownership 404).

## Global Constraints

- **No new migration** — `user_injuries` (id, user_id FK ON DELETE CASCADE, body_region NOT NULL, severity CHECK IN mild/moderate/severe, active NOT NULL DEFAULT true, notes, created_at) already exists.
- **Ownership (D5, contract):** `PATCH`/`DELETE` an injury not owned by the caller (or absent) → **404**, never leak existence.
- **Validation (domain):** `severity` ∈ {mild,moderate,severe}; `body_region` non-empty. `body_region` is free-text in v1 (controlled vocabulary deferred to Slice 3 catalog).
- **Build:** from `Server/`, `C:\msys64\ucrt64\bin` on PATH. `cmake --build --preset default [--target kinetiq_tests|kinetiq_server]`. Run tests via `./build/kinetiq_tests.exe --gtest_filter=...`. DB tests need Postgres (`docker compose up -d`); they `GTEST_SKIP()` if down. When a build gates the next step, check the real exit code (`; echo exit=$?`), not piped `tail`.
- **Reuse:** `AutoCommitPg*` connection-per-call pattern for the production repo; `dto::errorResponse` (AuthDto.h) + `dto::validationErrorResponse` (ProfileDto.h) for error bodies; `authenticate()` (Authenticator.h) for auth.
- **File map (this stage):**
  - Create `Server/src/domain/Injury.h`
  - Create `Server/src/domain/InjuryPolicy.h` — `InjuryValidationError` + `requireValidInjury`
  - Create `Server/tests/test_injury_policy.cpp`
  - Create `Server/src/ports/IInjuryRepository.h`
  - Create `Server/tests/support/InMemoryInjuryRepository.h`
  - Create `Server/tests/test_injury_repository.cpp`
  - Create `Server/src/adapters/db/PgInjuryRepository.h` / `.cpp`
  - Create `Server/src/adapters/db/AutoCommitPgInjuryRepository.h` / `.cpp`
  - Create `Server/tests/test_pg_injury_repository.cpp`
  - Create `Server/tests/test_auto_commit_pg_injury_repository.cpp`
  - Create `Server/src/app/InjuryService.h`
  - Create `Server/tests/test_injury_service.cpp`
  - Create `Server/src/http/dto/InjuryDto.h`
  - Create `Server/src/http/controllers/InjuryController.h` / `.cpp`
  - Create `Server/tests/test_injury_controller.cpp`
  - Modify `Server/src/main.cpp` — register injury routes
  - Modify `Server/CMakeLists.txt` — new sources + tests

---

### Task B1: Injury domain type + validation

**Files:**
- Create: `Server/src/domain/Injury.h`, `Server/src/domain/InjuryPolicy.h`
- Test: `Server/tests/test_injury_policy.cpp`
- Modify: `Server/CMakeLists.txt` (add test source)

**Interfaces:**
- Produces: `domain::Injury` struct; `domain::InjuryValidationError` (has `field()`); `domain::requireValidInjury(const domain::Injury&)`.

- [ ] **Step 1: Write `Injury.h`**

Create `Server/src/domain/Injury.h`:

```cpp
#pragma once

#include <optional>
#include <string>

namespace kinetiq::domain {

// A user's injury. Drives the Phase 3 rule-based safety filter (body_region is
// JOINed against exercise_contraindications). Inactive injuries are retained
// for history but excluded from the filter.
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;             // free text in v1 (e.g. "lower_back")
    std::string severity;               // mild|moderate|severe
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;              // ISO timestamp, set by the DB (read-only)
};

}  // namespace kinetiq::domain
```

- [ ] **Step 2: Write the failing test**

Create `Server/tests/test_injury_policy.cpp`:

```cpp
// Slice 2 (#20): InjuryPolicy -- pure validation of an injury's fields,
// mirroring ProfilePolicy. requireValidInjury throws InjuryValidationError
// (carrying the offending field name) on the first invalid field.

#include <gtest/gtest.h>

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"

using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::domain::requireValidInjury;

namespace {

Injury validInjury() {
    Injury i;
    i.userId = 1;
    i.bodyRegion = "lower_back";
    i.severity = "moderate";
    i.active = true;
    return i;
}

}  // namespace

TEST(InjuryPolicy, ValidInjury_DoesNotThrow) {
    EXPECT_NO_THROW(requireValidInjury(validInjury()));
}

TEST(InjuryPolicy, EmptyBodyRegion_Throws_FieldIsBodyRegion) {
    Injury i = validInjury();
    i.bodyRegion = "";
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "body_region");
    }
}

TEST(InjuryPolicy, BadSeverity_Throws_FieldIsSeverity) {
    Injury i = validInjury();
    i.severity = "critical";
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "severity");
    }
}

TEST(InjuryPolicy, EmptySeverity_Throws) {
    Injury i = validInjury();
    i.severity = "";
    EXPECT_THROW(requireValidInjury(i), InjuryValidationError);
}

TEST(InjuryPolicy, AllThreeSeverities_Accepted) {
    for (const char* s : {"mild", "moderate", "severe"}) {
        Injury i = validInjury();
        i.severity = s;
        EXPECT_NO_THROW(requireValidInjury(i)) << s;
    }
}
```

- [ ] **Step 3: Register in CMake, verify red**

In `Server/CMakeLists.txt`, in the `kinetiq_tests` source list, add after `tests/test_profile_policy.cpp`:

```cmake
        tests/test_injury_policy.cpp
```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — `fatal error: domain/InjuryPolicy.h: No such file or directory`.

- [ ] **Step 4: Write `InjuryPolicy.h`**

Create `Server/src/domain/InjuryPolicy.h`:

```cpp
#pragma once

#include "domain/Injury.h"

#include <initializer_list>
#include <stdexcept>
#include <string>

namespace kinetiq::domain {

// Thrown when an injury field is invalid. Carries the offending field's wire
// name (e.g. "severity") for the RFC 9457 errors object.
class InjuryValidationError : public std::runtime_error {
public:
    InjuryValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

// Validates an injury's fields. Throws InjuryValidationError on the first
// invalid field; returns normally if valid.
inline void requireValidInjury(const Injury& injury) {
    if (injury.bodyRegion.empty()) {
        throw InjuryValidationError("body_region", "body_region must not be empty");
    }
    const bool severityOk = injury.severity == "mild" || injury.severity == "moderate" ||
                            injury.severity == "severe";
    if (!severityOk) {
        throw InjuryValidationError("severity", "severity must be mild, moderate, or severe");
    }
}

}  // namespace kinetiq::domain
```

- [ ] **Step 5: Build + run — verify green**

Run: `cmake --build --preset default --target kinetiq_tests` then
`./build/kinetiq_tests.exe --gtest_filter='InjuryPolicy.*'`
Expected: **PASS** — 5/5.

- [ ] **Step 6: Commit**

```
git add Server/src/domain/Injury.h Server/src/domain/InjuryPolicy.h Server/tests/test_injury_policy.cpp Server/CMakeLists.txt
git commit -m "feat(server): Injury domain type + InjuryPolicy validation (TDD) #20"
```

---

### Task B2: `IInjuryRepository` port + in-memory fake

**Files:**
- Create: `Server/src/ports/IInjuryRepository.h`, `Server/tests/support/InMemoryInjuryRepository.h`
- Test: `Server/tests/test_injury_repository.cpp`
- Modify: `Server/CMakeLists.txt` (add test source)

**Interfaces:**
- Consumes: `domain::Injury`.
- Produces: `ports::IInjuryRepository` with `create(Injury)->Injury` (assigns id), `listByUser(long long)->std::vector<Injury>`, `findById(long long)->std::optional<Injury>`, `update(const Injury&)->void`, `deleteById(long long)->void`; `testsupport::InMemoryInjuryRepository` implementing it.

- [ ] **Step 1: Write the failing test**

Create `Server/tests/test_injury_repository.cpp`:

```cpp
// Slice 2 (#20): IInjuryRepository, exercised against the in-memory fake. The
// real adapter (PgInjuryRepository) must satisfy the same contract.

#include <gtest/gtest.h>

#include "domain/Injury.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::domain::Injury;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

Injury make(long long userId, const std::string& region, const std::string& severity) {
    Injury i;
    i.userId = userId;
    i.bodyRegion = region;
    i.severity = severity;
    return i;
}

}  // namespace

TEST(InMemoryInjuryRepositoryTest, Create_AssignsIdAndPersists) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));
    EXPECT_GT(saved.id, 0);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->bodyRegion, "lower_back");
    EXPECT_EQ(found->userId, 1);
}

TEST(InMemoryInjuryRepositoryTest, ListByUser_ReturnsOnlyThatUsersInjuries) {
    InMemoryInjuryRepository repo;
    repo.create(make(1, "lower_back", "mild"));
    repo.create(make(1, "left_knee", "moderate"));
    repo.create(make(2, "right_shoulder", "severe"));

    const auto forUser1 = repo.listByUser(1);
    EXPECT_EQ(forUser1.size(), 2u);
    for (const auto& i : forUser1) EXPECT_EQ(i.userId, 1);
}

TEST(InMemoryInjuryRepositoryTest, FindById_UnknownId_ReturnsNullopt) {
    InMemoryInjuryRepository repo;
    EXPECT_FALSE(repo.findById(999).has_value());
}

TEST(InMemoryInjuryRepositoryTest, Update_ChangesStoredFields) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));

    Injury modified = saved;
    modified.active = false;
    modified.severity = "mild";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->active);
    EXPECT_EQ(found->severity, "mild");
}

TEST(InMemoryInjuryRepositoryTest, DeleteById_RemovesInjury) {
    InMemoryInjuryRepository repo;
    const Injury saved = repo.create(make(1, "lower_back", "moderate"));
    repo.deleteById(saved.id);
    EXPECT_FALSE(repo.findById(saved.id).has_value());
    EXPECT_TRUE(repo.listByUser(1).empty());
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt` `kinetiq_tests` list, add after `tests/test_injury_policy.cpp`:

```cmake
        tests/test_injury_repository.cpp
```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — `fatal error: support/InMemoryInjuryRepository.h: No such file or directory`.

- [ ] **Step 3: Write the port**

Create `Server/src/ports/IInjuryRepository.h`:

```cpp
#pragma once

#include "domain/Injury.h"

#include <optional>
#include <vector>

namespace kinetiq::ports {

// Repository port for User_Injuries persistence. Implementations:
// InMemoryInjuryRepository (tests/support/, fake) and PgInjuryRepository
// (adapters/db/, real) -- both satisfy this exact contract.
class IInjuryRepository {
public:
    virtual ~IInjuryRepository() = default;

    // Assigns an id (and created_at) and persists. Returns the stored injury.
    virtual domain::Injury create(const domain::Injury& injury) = 0;

    // All injuries belonging to userId (any active state), oldest first.
    virtual std::vector<domain::Injury> listByUser(long long userId) = 0;

    // Looks up by id regardless of owner (the caller checks ownership).
    virtual std::optional<domain::Injury> findById(long long id) = 0;

    // Overwrites the mutable fields (body_region, severity, active, notes) of
    // an existing injury matched by injury.id.
    virtual void update(const domain::Injury& injury) = 0;

    // Removes the injury with this id (no-op if it doesn't exist).
    virtual void deleteById(long long id) = 0;
};

}  // namespace kinetiq::ports
```

- [ ] **Step 4: Write the fake**

Create `Server/tests/support/InMemoryInjuryRepository.h`:

```cpp
#pragma once

#include "ports/IInjuryRepository.h"

#include <algorithm>
#include <optional>
#include <vector>

namespace kinetiq::testsupport {

// Hand-written in-memory fake for kinetiq::ports::IInjuryRepository. No gmock.
class InMemoryInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override {
        kinetiq::domain::Injury stored = injury;
        stored.id = nextId_++;
        if (stored.createdAt.empty()) stored.createdAt = "1970-01-01T00:00:00Z";
        injuries_.push_back(stored);
        return stored;
    }

    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override {
        std::vector<kinetiq::domain::Injury> out;
        for (const auto& i : injuries_) {
            if (i.userId == userId) out.push_back(i);
        }
        return out;
    }

    std::optional<kinetiq::domain::Injury> findById(long long id) override {
        for (const auto& i : injuries_) {
            if (i.id == id) return i;
        }
        return std::nullopt;
    }

    void update(const kinetiq::domain::Injury& injury) override {
        for (auto& i : injuries_) {
            if (i.id == injury.id) {
                i = injury;
                return;
            }
        }
    }

    void deleteById(long long id) override {
        injuries_.erase(std::remove_if(injuries_.begin(), injuries_.end(),
                                       [id](const kinetiq::domain::Injury& i) { return i.id == id; }),
                        injuries_.end());
    }

private:
    std::vector<kinetiq::domain::Injury> injuries_;
    long long nextId_ = 1;
};

}  // namespace kinetiq::testsupport
```

- [ ] **Step 5: Build + run — verify green**

Run: `cmake --build --preset default --target kinetiq_tests` then
`./build/kinetiq_tests.exe --gtest_filter='InMemoryInjuryRepositoryTest.*'`
Expected: **PASS** — 5/5.

- [ ] **Step 6: Commit**

```
git add Server/src/ports/IInjuryRepository.h Server/tests/support/InMemoryInjuryRepository.h Server/tests/test_injury_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): IInjuryRepository port + in-memory fake (TDD) #20"
```

---

### Task B3: `PgInjuryRepository` + `AutoCommitPgInjuryRepository`

**Files:**
- Create: `Server/src/adapters/db/PgInjuryRepository.h` / `.cpp`, `Server/src/adapters/db/AutoCommitPgInjuryRepository.h` / `.cpp`
- Test: `Server/tests/test_pg_injury_repository.cpp`, `Server/tests/test_auto_commit_pg_injury_repository.cpp`
- Modify: `Server/CMakeLists.txt` (both `.cpp` into `kinetiq_server` + `kinetiq_tests`; two test sources into `kinetiq_tests`)

**Interfaces:**
- Consumes: `ports::IInjuryRepository`, `domain::Injury`. `PgInjuryRepository(pqxx::work&)`; `AutoCommitPgInjuryRepository(std::string connString)`.
- Produces: real Postgres persistence of injuries.

- [ ] **Step 1: Write the failing DB test**

Create `Server/tests/test_pg_injury_repository.cpp`:

```cpp
// Slice 2 (#20): PgInjuryRepository against real Postgres, using DbTest's
// per-test transaction rollback. Injuries FK to users, so each test saves a
// user first (in the same transaction).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgInjuryRepository.h"
#include "adapters/db/PgUserRepository.h"
#include "domain/Injury.h"
#include "domain/User.h"

using kinetiq::db::PgInjuryRepository;
using kinetiq::db::PgUserRepository;
using kinetiq::domain::Injury;
using kinetiq::domain::User;

namespace {

long long seedUser(pqxx::work& tx, const std::string& email) {
    PgUserRepository users(tx);
    User u;
    u.email = email;
    u.passwordHash = "x";
    return users.save(u).id;
}

Injury make(long long userId, const std::string& region, const std::string& severity) {
    Injury i;
    i.userId = userId;
    i.bodyRegion = region;
    i.severity = severity;
    return i;
}

}  // namespace

TEST_F(DbTest, PgInjuryRepository_CreateThenListByUser) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const long long userId = seedUser(tx(), "pg-injury-a@example.com");
    PgInjuryRepository repo(tx());

    const Injury saved = repo.create(make(userId, "lower_back", "moderate"));
    EXPECT_GT(saved.id, 0);
    EXPECT_FALSE(saved.createdAt.empty());  // DB set created_at

    const auto list = repo.listByUser(userId);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
    EXPECT_EQ(list[0].severity, "moderate");
    EXPECT_TRUE(list[0].active);
}

TEST_F(DbTest, PgInjuryRepository_ListByUser_IsScopedToOwner) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const long long a = seedUser(tx(), "pg-injury-owner-a@example.com");
    const long long b = seedUser(tx(), "pg-injury-owner-b@example.com");
    PgInjuryRepository repo(tx());
    repo.create(make(a, "lower_back", "mild"));
    repo.create(make(b, "left_knee", "severe"));

    EXPECT_EQ(repo.listByUser(a).size(), 1u);
    EXPECT_EQ(repo.listByUser(b).size(), 1u);
}

TEST_F(DbTest, PgInjuryRepository_Update_ChangesFields) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const long long userId = seedUser(tx(), "pg-injury-upd@example.com");
    PgInjuryRepository repo(tx());
    const Injury saved = repo.create(make(userId, "lower_back", "moderate"));

    Injury modified = saved;
    modified.active = false;
    modified.severity = "mild";
    modified.notes = "healing";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->active);
    EXPECT_EQ(found->severity, "mild");
    EXPECT_EQ(found->notes, "healing");
}

TEST_F(DbTest, PgInjuryRepository_DeleteById_Removes) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const long long userId = seedUser(tx(), "pg-injury-del@example.com");
    PgInjuryRepository repo(tx());
    const Injury saved = repo.create(make(userId, "lower_back", "moderate"));

    repo.deleteById(saved.id);
    EXPECT_FALSE(repo.findById(saved.id).has_value());
}
```

Also create `Server/tests/test_auto_commit_pg_injury_repository.cpp`:

```cpp
// Slice 2 (#20): AutoCommitPgInjuryRepository -- the repo wired into main.cpp.
// Opens its own connections, so it cleans up after itself (no DbTest rollback).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgInjuryRepository.h"
#include "adapters/db/AutoCommitPgUserRepository.h"
#include "domain/Injury.h"
#include "domain/User.h"

#include <pqxx/pqxx>

using kinetiq::db::AutoCommitPgInjuryRepository;
using kinetiq::db::AutoCommitPgUserRepository;
using kinetiq::domain::Injury;
using kinetiq::domain::User;

TEST(AutoCommitPgInjuryRepositoryTest, CreateThenList_AcrossSeparateConnections) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }

    const std::string email = "auto-commit-injury@example.com";
    AutoCommitPgUserRepository users(kinetiq::testing::testConnString());
    User u;
    u.email = email;
    u.passwordHash = "x";
    const long long userId = users.save(u).id;

    AutoCommitPgInjuryRepository injuries(kinetiq::testing::testConnString());
    Injury i;
    i.userId = userId;
    i.bodyRegion = "lower_back";
    i.severity = "moderate";
    const Injury saved = injuries.create(i);

    const auto list = injuries.listByUser(userId);
    EXPECT_EQ(list.size(), 1u);

    injuries.deleteById(saved.id);
    // Deleting the user cascades, but be explicit for clarity.
    pqxx::connection conn(kinetiq::testing::testConnString());
    pqxx::work tx(conn);
    tx.exec("DELETE FROM users WHERE email = " + tx.quote(email));
    tx.commit();
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt`:
- `kinetiq_server` sources — after `src/adapters/db/AutoCommitPgUserRepository.cpp`:
  ```cmake
      src/adapters/db/PgInjuryRepository.cpp
      src/adapters/db/AutoCommitPgInjuryRepository.cpp
  ```
- `kinetiq_tests` sources — after its `src/adapters/db/AutoCommitPgUserRepository.cpp` line:
  ```cmake
        src/adapters/db/PgInjuryRepository.cpp
        src/adapters/db/AutoCommitPgInjuryRepository.cpp
  ```
- `kinetiq_tests` — after `tests/test_auto_commit_pg_user_repository.cpp`:
  ```cmake
        tests/test_pg_injury_repository.cpp
        tests/test_auto_commit_pg_injury_repository.cpp
  ```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — CMake `Cannot find source file: ...PgInjuryRepository.cpp` (they don't exist yet).

- [ ] **Step 3: Write `PgInjuryRepository.h`**

Create `Server/src/adapters/db/PgInjuryRepository.h`:

```cpp
#pragma once

#include "domain/Injury.h"
#include "ports/IInjuryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed IInjuryRepository. Operates within a transaction it is handed
// (composes with DbTest's rollback in tests, a per-op transaction in prod via
// AutoCommitPgInjuryRepository).
class PgInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    explicit PgInjuryRepository(pqxx::work& tx) : tx_(tx) {}

    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override;
    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override;
    std::optional<kinetiq::domain::Injury> findById(long long id) override;
    void update(const kinetiq::domain::Injury& injury) override;
    void deleteById(long long id) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
```

- [ ] **Step 4: Write `PgInjuryRepository.cpp`**

Create `Server/src/adapters/db/PgInjuryRepository.cpp`:

```cpp
#include "adapters/db/PgInjuryRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::Injury rowToInjury(const pqxx::row& row) {
    kinetiq::domain::Injury injury;
    injury.id = row["id"].as<long long>();
    injury.userId = row["user_id"].as<long long>();
    injury.bodyRegion = row["body_region"].as<std::string>();
    injury.severity = row["severity"].is_null() ? "" : row["severity"].as<std::string>();
    injury.active = row["active"].as<bool>();
    if (!row["notes"].is_null()) injury.notes = row["notes"].as<std::string>();
    injury.createdAt = row["created_at"].as<std::string>();
    return injury;
}

constexpr const char* kColumns =
    "id, user_id, body_region, severity, active, notes, created_at";

}  // namespace

kinetiq::domain::Injury PgInjuryRepository::create(const kinetiq::domain::Injury& injury) {
    const auto result = tx_.exec(
        "INSERT INTO user_injuries (user_id, body_region, severity, active, notes) "
        "VALUES ($1, $2, $3, $4, $5) "
        "RETURNING id, user_id, body_region, severity, active, notes, created_at",
        pqxx::params{injury.userId, injury.bodyRegion, injury.severity, injury.active,
                     injury.notes});
    return rowToInjury(result[0]);
}

std::vector<kinetiq::domain::Injury> PgInjuryRepository::listByUser(long long userId) {
    const auto result =
        tx_.exec("SELECT " + std::string(kColumns) +
                     " FROM user_injuries WHERE user_id = $1 ORDER BY id",
                 pqxx::params{userId});
    std::vector<kinetiq::domain::Injury> out;
    out.reserve(result.size());
    for (const auto& row : result) out.push_back(rowToInjury(row));
    return out;
}

std::optional<kinetiq::domain::Injury> PgInjuryRepository::findById(long long id) {
    const auto result = tx_.exec(
        "SELECT " + std::string(kColumns) + " FROM user_injuries WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToInjury(result[0]);
}

void PgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    tx_.exec(
        "UPDATE user_injuries SET body_region = $1, severity = $2, active = $3, notes = $4 "
        "WHERE id = $5",
        pqxx::params{injury.bodyRegion, injury.severity, injury.active, injury.notes, injury.id});
}

void PgInjuryRepository::deleteById(long long id) {
    tx_.exec("DELETE FROM user_injuries WHERE id = $1", pqxx::params{id});
}

}  // namespace kinetiq::db
```

- [ ] **Step 5: Write the `AutoCommit` wrapper (`.h` + `.cpp`)**

Create `Server/src/adapters/db/AutoCommitPgInjuryRepository.h`:

```cpp
#pragma once

#include "ports/IInjuryRepository.h"

#include <string>

namespace kinetiq::db {

// Production IInjuryRepository: opens a fresh connection + transaction per call,
// delegates to PgInjuryRepository, commits. Same rationale as
// AutoCommitPgUserRepository (a Crow route captures its deps for the server's
// lifetime; a pqxx transaction is per-operation and unsafe to share across
// concurrent requests).
class AutoCommitPgInjuryRepository : public kinetiq::ports::IInjuryRepository {
public:
    explicit AutoCommitPgInjuryRepository(std::string connString)
        : connString_(std::move(connString)) {}

    kinetiq::domain::Injury create(const kinetiq::domain::Injury& injury) override;
    std::vector<kinetiq::domain::Injury> listByUser(long long userId) override;
    std::optional<kinetiq::domain::Injury> findById(long long id) override;
    void update(const kinetiq::domain::Injury& injury) override;
    void deleteById(long long id) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
```

Create `Server/src/adapters/db/AutoCommitPgInjuryRepository.cpp`:

```cpp
#include "adapters/db/AutoCommitPgInjuryRepository.h"

#include "adapters/db/PgInjuryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

kinetiq::domain::Injury AutoCommitPgInjuryRepository::create(
    const kinetiq::domain::Injury& injury) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.create(injury);
    tx.commit();
    return result;
}

std::vector<kinetiq::domain::Injury> AutoCommitPgInjuryRepository::listByUser(long long userId) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.listByUser(userId);
    tx.commit();
    return result;
}

std::optional<kinetiq::domain::Injury> AutoCommitPgInjuryRepository::findById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    auto result = repo.findById(id);
    tx.commit();
    return result;
}

void AutoCommitPgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    repo.update(injury);
    tx.commit();
}

void AutoCommitPgInjuryRepository::deleteById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgInjuryRepository repo(tx);
    repo.deleteById(id);
    tx.commit();
}

}  // namespace kinetiq::db
```

- [ ] **Step 6: Build + run — verify green (Postgres up)**

Run: `cmake --build --preset default --target kinetiq_tests` then
`./build/kinetiq_tests.exe --gtest_filter='*InjuryRepository*'`
Expected: **PASS** — the fake tests + 4 `PgInjuryRepository` DB tests + the auto-commit test.

- [ ] **Step 7: Commit**

```
git add Server/src/adapters/db/PgInjuryRepository.h Server/src/adapters/db/PgInjuryRepository.cpp Server/src/adapters/db/AutoCommitPgInjuryRepository.h Server/src/adapters/db/AutoCommitPgInjuryRepository.cpp Server/tests/test_pg_injury_repository.cpp Server/tests/test_auto_commit_pg_injury_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): PgInjuryRepository + AutoCommit wrapper (TDD) #20"
```

---

### Task B4: `app::InjuryService` (with ownership 404)

**Files:**
- Create: `Server/src/app/InjuryService.h`
- Test: `Server/tests/test_injury_service.cpp`
- Modify: `Server/CMakeLists.txt` (add test source)

**Interfaces:**
- Consumes: `ports::IInjuryRepository`; `domain::requireValidInjury`.
- Produces:
  - `struct app::AddInjuryRequest { long long userId; std::string bodyRegion; std::string severity; std::optional<std::string> notes; };`
  - `struct app::UpdateInjuryFields { std::optional<std::string> bodyRegion, severity, notes; std::optional<bool> active; };`
  - `class app::InjuryNotFoundError : public std::runtime_error;`
  - `class app::InjuryService { explicit InjuryService(ports::IInjuryRepository&); domain::Injury add(const AddInjuryRequest&); std::vector<domain::Injury> list(long long userId); domain::Injury update(long long userId, long long injuryId, const UpdateInjuryFields&); void remove(long long userId, long long injuryId); };`

- [ ] **Step 1: Write the failing test**

Create `Server/tests/test_injury_service.cpp`:

```cpp
// Slice 2 (#20): InjuryService -- CRUD over IInjuryRepository with an ownership
// check (another user's injury is invisible: throws InjuryNotFoundError).

#include <gtest/gtest.h>

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::app::AddInjuryRequest;
using kinetiq::app::InjuryNotFoundError;
using kinetiq::app::InjuryService;
using kinetiq::app::UpdateInjuryFields;
using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

AddInjuryRequest addReq(long long userId, const std::string& region, const std::string& severity) {
    AddInjuryRequest r;
    r.userId = userId;
    r.bodyRegion = region;
    r.severity = severity;
    return r;
}

}  // namespace

TEST(InjuryService, Add_ValidInjury_CreatesAndAppearsInList) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);

    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_GT(created.id, 0);

    const auto list = svc.list(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
}

TEST(InjuryService, Add_BadSeverity_Throws) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    EXPECT_THROW(svc.add(addReq(1, "lower_back", "critical")), InjuryValidationError);
}

TEST(InjuryService, Update_OwnInjury_ChangesFields) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    const Injury updated = svc.update(1, created.id, fields);
    EXPECT_FALSE(updated.active);
}

TEST(InjuryService, Update_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_THROW(svc.update(2, created.id, fields), InjuryNotFoundError);  // user 2 != owner 1
}

TEST(InjuryService, Remove_OwnInjury_Deletes) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    svc.remove(1, created.id);
    EXPECT_TRUE(svc.list(1).empty());
}

TEST(InjuryService, Remove_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InjuryService svc(repo);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_THROW(svc.remove(2, created.id), InjuryNotFoundError);
    EXPECT_EQ(svc.list(1).size(), 1u);  // still there
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt` `kinetiq_tests` list, add after `tests/test_injury_repository.cpp`:

```cmake
        tests/test_injury_service.cpp
```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — `fatal error: app/InjuryService.h: No such file or directory`.

- [ ] **Step 3: Write `InjuryService.h`**

Create `Server/src/app/InjuryService.h`:

```cpp
#pragma once

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"
#include "ports/IInjuryRepository.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace kinetiq::app {

// Thrown when an injury doesn't exist OR isn't owned by the requesting user.
// Deliberately one type for both -- the HTTP layer maps it to 404 so another
// user's injury is indistinguishable from a nonexistent one (no existence leak).
class InjuryNotFoundError : public std::runtime_error {
public:
    InjuryNotFoundError() : std::runtime_error("injury not found") {}
};

struct AddInjuryRequest {
    long long userId = 0;
    std::string bodyRegion;
    std::string severity;
    std::optional<std::string> notes;
};

// Partial update: present fields change, absent fields are left unchanged.
struct UpdateInjuryFields {
    std::optional<std::string> bodyRegion;
    std::optional<std::string> severity;
    std::optional<std::string> notes;
    std::optional<bool> active;
};

// Cohesive CRUD service for a user's injuries. One class (not four use-case
// classes) because the four operations share the ownership check (D5).
class InjuryService {
public:
    explicit InjuryService(kinetiq::ports::IInjuryRepository& injuries) : injuries_(injuries) {}

    kinetiq::domain::Injury add(const AddInjuryRequest& req) {
        kinetiq::domain::Injury injury;
        injury.userId = req.userId;
        injury.bodyRegion = req.bodyRegion;
        injury.severity = req.severity;
        injury.notes = req.notes;
        injury.active = true;
        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        return injuries_.create(injury);
    }

    std::vector<kinetiq::domain::Injury> list(long long userId) {
        return injuries_.listByUser(userId);
    }

    kinetiq::domain::Injury update(long long userId, long long injuryId,
                                   const UpdateInjuryFields& fields) {
        kinetiq::domain::Injury injury = requireOwned(userId, injuryId);
        if (fields.bodyRegion) injury.bodyRegion = *fields.bodyRegion;
        if (fields.severity) injury.severity = *fields.severity;
        if (fields.notes) injury.notes = fields.notes;
        if (fields.active) injury.active = *fields.active;
        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        injuries_.update(injury);
        return injury;
    }

    void remove(long long userId, long long injuryId) {
        requireOwned(userId, injuryId);
        injuries_.deleteById(injuryId);
    }

private:
    // Loads the injury and asserts the caller owns it, else throws
    // InjuryNotFoundError (missing and not-owned are treated identically).
    kinetiq::domain::Injury requireOwned(long long userId, long long injuryId) {
        auto found = injuries_.findById(injuryId);
        if (!found.has_value() || found->userId != userId) {
            throw InjuryNotFoundError();
        }
        return *found;
    }

    kinetiq::ports::IInjuryRepository& injuries_;
};

}  // namespace kinetiq::app
```

- [ ] **Step 4: Build + run — verify green**

Run: `cmake --build --preset default --target kinetiq_tests` then
`./build/kinetiq_tests.exe --gtest_filter='InjuryService.*'`
Expected: **PASS** — 6/6.

- [ ] **Step 5: Commit**

```
git add Server/src/app/InjuryService.h Server/tests/test_injury_service.cpp Server/CMakeLists.txt
git commit -m "feat(server): InjuryService with ownership 404 (TDD) #20"
```

---

### Task B5: HTTP layer — `InjuryDto` + `InjuryController`

**Files:**
- Create: `Server/src/http/dto/InjuryDto.h`, `Server/src/http/controllers/InjuryController.h` / `.cpp`
- Test: `Server/tests/test_injury_controller.cpp`
- Modify: `Server/CMakeLists.txt` (`InjuryController.cpp` into `kinetiq_server` + `kinetiq_tests`; test into `kinetiq_tests`)

**Interfaces:**
- Consumes: `app::InjuryService`/`AddInjuryRequest`/`UpdateInjuryFields`/`InjuryNotFoundError`; `domain::InjuryValidationError`; `http::authenticate`; `dto::errorResponse` (AuthDto.h); `dto::validationErrorResponse` (ProfileDto.h).
- Produces: `dto::parseAddInjury(body, userId)`, `dto::parseUpdateInjury(body)`, `dto::injuryToJson(Injury)`, `dto::injuryListToJson(vector<Injury>)`; `http::registerInjuryRoutes(crow::SimpleApp&, ports::IInjuryRepository&, ports::ITokenService&)`.

- [ ] **Step 1: Write the failing HTTP test**

Create `Server/tests/test_injury_controller.cpp`:

```cpp
// Slice 2 (#20): HTTP layer for /users/me/injuries CRUD. Drives real routing
// via HttpClient. Registers auth + injury routes; two users prove ownership
// isolation (another user's injury -> 404).

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/InjuryController.h"
#include "support/InMemoryInjuryRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryInjuryRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct InjuryApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryInjuryRepository injuries;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    InjuryApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerInjuryRoutes(app, injuries, tokens);
    }
};

template <typename App>
std::string registerAndGetToken(kinetiq::testing::HttpClient<App>& client,
                                const std::string& email) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":")" + email + R"(","password":"StrongPass1","display_name":"T"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(InjuryControllerTest, Create_ValidInjury_Returns201AndAppearsInList) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-a@example.com");

    const auto created = client.post("/api/v1/users/me/injuries",
                                     R"({"body_region":"lower_back","severity":"moderate"})",
                                     {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(created.code, 201);

    const auto list = client.get("/api/v1/users/me/injuries", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(list.code, 200);
    const auto body = crow::json::load(list.body);
    ASSERT_TRUE(body);
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(std::string(body[0]["body_region"].s()), "lower_back");
}

TEST(InjuryControllerTest, Create_BadSeverity_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-b@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"body_region":"lower_back","severity":"critical"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_Unauthenticated_Returns401) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"body_region":"lower_back","severity":"mild"})");
    EXPECT_EQ(r.code, 401);
}

TEST(InjuryControllerTest, Patch_OwnInjury_Returns200) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-c@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"active":false})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
}

TEST(InjuryControllerTest, Patch_OtherUsersInjury_Returns404) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string tokenA = registerAndGetToken(client, "inj-owner@example.com");
    const std::string tokenB = registerAndGetToken(client, "inj-other@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate"})",
                  {{"Authorization", "Bearer " + tokenA}})
            .body);
    const long long id = created["id"].i();

    // User B tries to edit user A's injury.
    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"active":false})", {{"Authorization", "Bearer " + tokenB}});
    EXPECT_EQ(r.code, 404);
}

TEST(InjuryControllerTest, Delete_OwnInjury_Returns204AndGone) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-d@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto del = client.send(crow::HTTPMethod::Delete,
                                 "/api/v1/users/me/injuries/" + std::to_string(id), "",
                                 {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(del.code, 204);

    const auto list = crow::json::load(
        client.get("/api/v1/users/me/injuries", {{"Authorization", "Bearer " + token}}).body);
    EXPECT_EQ(list.size(), 0u);
}

TEST(InjuryControllerTest, Delete_OtherUsersInjury_Returns404) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string tokenA = registerAndGetToken(client, "inj-del-owner@example.com");
    const std::string tokenB = registerAndGetToken(client, "inj-del-other@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate"})",
                  {{"Authorization", "Bearer " + tokenA}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Delete,
                               "/api/v1/users/me/injuries/" + std::to_string(id), "",
                               {{"Authorization", "Bearer " + tokenB}});
    EXPECT_EQ(r.code, 404);
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt`:
- `kinetiq_server` sources — after `src/http/controllers/UserController.cpp`:
  ```cmake
      src/http/controllers/InjuryController.cpp
  ```
- `kinetiq_tests` sources — after its `src/http/controllers/UserController.cpp` line:
  ```cmake
        src/http/controllers/InjuryController.cpp
  ```
- `kinetiq_tests` — after `tests/test_user_controller.cpp`:
  ```cmake
        tests/test_injury_controller.cpp
  ```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — CMake can't find `InjuryController.cpp` / missing `InjuryController.h`.

- [ ] **Step 3: Write `InjuryDto.h`**

Create `Server/src/http/dto/InjuryDto.h`:

```cpp
#pragma once

#include "app/InjuryService.h"
#include "domain/Injury.h"

#include "crow_all.h"

#include <string>

namespace kinetiq::http::dto {

// Parses POST body into an AddInjuryRequest, pairing it with the authenticated
// user id (never from the body).
inline kinetiq::app::AddInjuryRequest parseAddInjury(const crow::json::rvalue& body,
                                                     long long userId) {
    kinetiq::app::AddInjuryRequest req;
    req.userId = userId;
    req.bodyRegion = std::string(body["body_region"].s());
    req.severity = std::string(body["severity"].s());
    if (body.has("notes")) req.notes = std::string(body["notes"].s());
    return req;
}

// Parses PATCH body into UpdateInjuryFields -- only present keys are set.
inline kinetiq::app::UpdateInjuryFields parseUpdateInjury(const crow::json::rvalue& body) {
    kinetiq::app::UpdateInjuryFields fields;
    if (body.has("body_region")) fields.bodyRegion = std::string(body["body_region"].s());
    if (body.has("severity")) fields.severity = std::string(body["severity"].s());
    if (body.has("notes")) fields.notes = std::string(body["notes"].s());
    if (body.has("active")) fields.active = body["active"].b();
    return fields;
}

inline crow::json::wvalue injuryToJson(const kinetiq::domain::Injury& injury) {
    crow::json::wvalue json;
    json["id"] = injury.id;
    json["user_id"] = injury.userId;
    json["body_region"] = injury.bodyRegion;
    json["severity"] = injury.severity;
    json["active"] = injury.active;
    json["notes"] = injury.notes ? crow::json::wvalue(*injury.notes) : crow::json::wvalue(nullptr);
    json["created_at"] = injury.createdAt;
    return json;
}

inline crow::json::wvalue injuryListToJson(const std::vector<kinetiq::domain::Injury>& injuries) {
    crow::json::wvalue json = crow::json::wvalue::list();
    for (std::size_t idx = 0; idx < injuries.size(); ++idx) {
        json[idx] = injuryToJson(injuries[idx]);
    }
    return json;
}

}  // namespace kinetiq::http::dto
```

- [ ] **Step 4: Write `InjuryController.h`**

Create `Server/src/http/controllers/InjuryController.h`:

```cpp
#pragma once

#include "ports/IInjuryRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers CRUD routes for /api/v1/users/me/injuries on `app`. Free function
// (matches AuthController/UserController). Depends only on ports.
void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
```

- [ ] **Step 5: Write `InjuryController.cpp`**

Create `Server/src/http/controllers/InjuryController.cpp`. Two routes: the collection (`GET` list, `POST` create) and the item (`PATCH`, `DELETE` by `<int>` id). Crow binds `<int>` as `std::int64_t`, passed after `req`.

```cpp
#include "http/controllers/InjuryController.h"

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "http/dto/AuthDto.h"      // dto::errorResponse
#include "http/dto/InjuryDto.h"
#include "http/dto/ProfileDto.h"   // dto::validationErrorResponse
#include "http/middleware/Authenticator.h"

#include <cstdint>

namespace kinetiq::http {

void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::ITokenService& tokens) {
    // Collection: GET (list) + POST (create).
    CROW_ROUTE(app, "/api/v1/users/me/injuries")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
            [&injuries, &tokens](const crow::request& req) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries);

                if (req.method == crow::HTTPMethod::Get) {
                    return crow::response(200,
                                          dto::injuryListToJson(service.list(currentUser->userId)));
                }

                // POST
                const auto body = crow::json::load(req.body);
                if (!body) {
                    return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                              "Request body is not valid JSON.");
                }
                try {
                    const auto created =
                        service.add(dto::parseAddInjury(body, currentUser->userId));
                    return crow::response(201, dto::injuryToJson(created));
                } catch (const kinetiq::domain::InjuryValidationError& e) {
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const std::exception&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });

    // Item: PATCH + DELETE by id.
    CROW_ROUTE(app, "/api/v1/users/me/injuries/<int>")
        .methods(crow::HTTPMethod::Patch, crow::HTTPMethod::Delete)(
            [&injuries, &tokens](const crow::request& req, std::int64_t id) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries);

                try {
                    if (req.method == crow::HTTPMethod::Delete) {
                        service.remove(currentUser->userId, id);
                        return crow::response(204);
                    }
                    // PATCH
                    const auto body = crow::json::load(req.body);
                    if (!body) {
                        return dto::errorResponse(400, "malformed_request", "Malformed Request",
                                                  "Request body is not valid JSON.");
                    }
                    const auto updated =
                        service.update(currentUser->userId, id, dto::parseUpdateInjury(body));
                    return crow::response(200, dto::injuryToJson(updated));
                } catch (const kinetiq::app::InjuryNotFoundError&) {
                    return dto::errorResponse(404, "not_found", "Not Found",
                                              "Injury not found.");
                } catch (const kinetiq::domain::InjuryValidationError& e) {
                    return dto::validationErrorResponse(e.field(), e.what());
                } catch (const std::exception&) {
                    return dto::errorResponse(422, "validation_error", "Validation Failed",
                                              "One or more fields are missing or invalid.");
                }
            });
}

}  // namespace kinetiq::http
```

- [ ] **Step 6: Build + run — verify green**

Run: `cmake --build --preset default --target kinetiq_tests` then
`./build/kinetiq_tests.exe --gtest_filter='InjuryControllerTest.*'`
Expected: **PASS** — 7/7 (incl. both cross-user 404 cases).

- [ ] **Step 7: Full-suite regression check**

Run: `cmake --build --preset default; echo exit=$?` (confirm `exit=0`), then `./build/kinetiq_tests.exe`
Expected: all green (previous + injury tests). DB tests run with Postgres up.

- [ ] **Step 8: Commit**

```
git add Server/src/http/dto/InjuryDto.h Server/src/http/controllers/InjuryController.h Server/src/http/controllers/InjuryController.cpp Server/tests/test_injury_controller.cpp Server/CMakeLists.txt
git commit -m "feat(server): /users/me/injuries CRUD HTTP layer (TDD) #20"
```

---

### Task B6: Wire into `main.cpp` + live-verify

**Files:**
- Modify: `Server/src/main.cpp`

**Interfaces:**
- Consumes: `http::registerInjuryRoutes`; a new `AutoCommitPgInjuryRepository` local; the existing `tokens`.

- [ ] **Step 1: Add the include**

In `Server/src/main.cpp`, after `#include "http/controllers/UserController.h"`:

```cpp
#include "http/controllers/InjuryController.h"
```

And after `#include "adapters/db/AutoCommitPgUserRepository.h"`:

```cpp
#include "adapters/db/AutoCommitPgInjuryRepository.h"
```

- [ ] **Step 2: Construct the repo + register routes**

In `main.cpp`, after the `kinetiq::db::AutoCommitPgUserRepository users(buildConnString());` line, add:

```cpp
    kinetiq::db::AutoCommitPgInjuryRepository injuries(buildConnString());
```

After the `kinetiq::http::registerUserRoutes(app, users, tokens);` line, add:

```cpp
    kinetiq::http::registerInjuryRoutes(app, injuries, tokens);
```

- [ ] **Step 3: Build the server**

Run: `cmake --build --preset default --target kinetiq_server; echo exit=$?`
Expected: `exit=0`.

- [ ] **Step 4: Live-verify (Postgres up, server via `./scripts/run-dev.ps1 -Target server` or the exe)**

Register to get `$TOKEN`, then:

```
# create injury (expect 201, returns id)
curl -s -X POST localhost:8080/api/v1/users/me/injuries -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{"body_region":"lower_back","severity":"moderate"}'

# list (expect the one injury)
curl -s localhost:8080/api/v1/users/me/injuries -H "Authorization: Bearer $TOKEN"

# bad severity (expect 422)
curl -s -o /dev/null -w "%{http_code}\n" -X POST localhost:8080/api/v1/users/me/injuries \
  -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"body_region":"neck","severity":"critical"}'

# PATCH active=false on the returned id (expect 200)
curl -s -X PATCH localhost:8080/api/v1/users/me/injuries/<ID> -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{"active":false}'

# DELETE it (expect 204)
curl -s -o /dev/null -w "%{http_code}\n" -X DELETE localhost:8080/api/v1/users/me/injuries/<ID> \
  -H "Authorization: Bearer $TOKEN"

# no-token list (expect 401)
curl -s -o /dev/null -w "%{http_code}\n" localhost:8080/api/v1/users/me/injuries
```

Confirm each; delete the test user from the dev DB (cascades injuries) and stop the server.

- [ ] **Step 5: Commit**

```
git add Server/src/main.cpp
git commit -m "feat(server): wire /users/me/injuries routes into main; live-verified #20"
```

---

## Stage B done — definition of done

- Injuries CRUD implemented; `ctest` fully green (DB tests pass with Postgres up); ownership 404 verified in both HTTP tests and live curl; live-verified end to end.
- **Next:** Stage C (`#25` frontend). Update `WORKLOG.md` / `CONTEXT.md` and the living `docs/phase-2-slice-2-profile-injuries.md` build log.

---

## Self-review notes (author check against the spec)

- **Spec coverage:** `#20` list/create/update/delete ✓ (B2/B3/B4/B5); ownership 404 D5 ✓ (`InjuryService::requireOwned` + `InjuryNotFoundError` → 404 in B5, tested cross-user in B5 Step 1); one cohesive `InjuryService` D5 ✓ (B4); domain validation ✓ (B1); `body_region` free-text (Slice-3 vocabulary deferral) noted ✓; `AutoCommit` reuse ✓ (B3).
- **Type consistency:** `AddInjuryRequest`/`UpdateInjuryFields`/`InjuryNotFoundError`/`InjuryService` signatures identical across B4 (definition), B5 (`parseAddInjury`/`parseUpdateInjury` + controller usage). `IInjuryRepository` method set identical across port (B2), fake (B2), Pg adapter (B3), AutoCommit (B3). `requireValidInjury`/`InjuryValidationError::field()` consistent B1→B4→B5.
- **Watch items:**
  - Crow `<int>` route param binds as `std::int64_t` passed *after* `const crow::request&`. If the compiler rejects the handler arity/type, adjust the param type to what the vendored Crow expects (check `crow_all.h` for the routing tag→type mapping) — the ids are `long long` in the domain.
  - `crow::json::wvalue::list()` builds a JSON array; if that factory isn't in the vendored Crow, initialize via `crow::json::wvalue x; x[0] = ...;` (assigning an index makes it a list) or return `{"items": [...]}` — but the test asserts a top-level array, so keep it an array.
  - `body["active"].b()` reads a JSON bool; confirm `.b()` exists in the vendored Crow rvalue API (it does in standard Crow) — otherwise use `body["active"].t() == crow::json::type::True`.
