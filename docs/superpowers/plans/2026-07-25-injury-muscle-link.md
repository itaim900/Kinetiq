# Injury/Muscle Link Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Injuries can optionally reference a specific muscle from a small seeded catalog
(`injury_type == "muscle"` + `muscle_id`), while bone/tendon/joint/other injuries stay fully
supported without a muscle link. A read-only `GET /muscles` endpoint exposes the catalog.

**Architecture:** Extends the already-shipped Stage B injuries stack. New parallel read-only
`IMuscleRepository` port/adapters (mirrors `IUserRepository`/`IInjuryRepository`); `Injury` grows
two fields; `InjuryPolicy` gains an enum + cross-field rule; `InjuryService` gains muscle-existence
validation and an update cascade; HTTP layer extended + a new `MuscleController`.

**Tech Stack:** C++20, Crow (vendored `crow_all.h`), libpqxx, GoogleTest, CMake+Ninja (MSYS2 UCRT64 GCC).

> **Execution note (2026-10-21):** Task 1 alone leaves 10 service/controller tests red (empty
> `injury_type` now fails validation), which the new pre-commit hook blocks. To keep every commit
> green, **Tasks 1 + 2 were done together, plus the `injury_type`-only plumbing pulled forward from
> Tasks 5 and 7** (`AddInjuryRequest.injuryType`, `UpdateInjuryFields.injuryType`, DTO parse/emit,
> test fixtures + `Add_BadInjuryType`, `Update_ChangesInjuryType`, `Create_BadInjuryType_Returns422`).
> When doing Tasks 5 and 7, skip anything already present and add only the `muscle_id` parts.
> Also: Task 1's expected count "16/16" is really 10/10 (5 existing + 5 new).

**Spec:** [`docs/superpowers/specs/2026-07-25-injury-muscle-link-design.md`](../specs/2026-07-25-injury-muscle-link-design.md) (decisions E1–E6)

## Global Constraints

- **`body_region` is untouched** — still drives the Phase 3 safety filter unchanged; this feature is additive.
- **`muscle_id` is nullable and only valid when `injury_type == "muscle"`** — enforced by a DB `CHECK`, the domain policy, and the service (E1, E4).
- **`muscle_id` is never required**, even when `injury_type == "muscle"` (E4).
- **One muscle per injury** — a single nullable FK column, no join table (E2).
- **Migration seeds 16 muscles** using the exact names from `01-data-model.md`'s documented Phase 2 list, `ON CONFLICT (name) DO NOTHING` (E3).
- **Update cascade:** changing `injury_type` away from `"muscle"` auto-clears `muscle_id`, applied after the payload's fields, before validation/persist (E5).
- **`GET /muscles` is read-only and 🔒** like the rest of the API (E6).
- **Build:** from `Server/`, `C:\msys64\ucrt64\bin` on PATH. `cmake --build --preset default [--target kinetiq_tests|kinetiq_server]`. Run tests via `./build/kinetiq_tests.exe --gtest_filter=...`. DB tests need Postgres up (`docker compose up -d`); `ensureTestDatabaseReady()` applies pending migrations automatically at `DbTest` suite setup — **but on failure it catches the exception and silently marks the suite unavailable (tests SKIP, not FAIL)**, so a broken migration doesn't show as red by default. Verify the migration explicitly via `psql` (Task 1) rather than relying on ctest output alone. When a build's success gates the next step, check the real exit code (`; echo exit=$?`), not piped `tail`.
- **File map (this plan):**
  - Create `Server/migrations/0003_injury_muscle_link.sql`
  - Create `Server/src/domain/Muscle.h`
  - Modify `Server/src/domain/Injury.h`, `Server/src/domain/InjuryPolicy.h`
  - Modify `Server/tests/test_injury_policy.cpp`
  - Modify `Server/src/adapters/db/PgInjuryRepository.cpp`
  - Modify `Server/tests/test_injury_repository.cpp`, `test_pg_injury_repository.cpp`, `test_auto_commit_pg_injury_repository.cpp`
  - Create `Server/src/ports/IMuscleRepository.h`
  - Create `Server/tests/support/InMemoryMuscleRepository.h`
  - Create `Server/tests/test_muscle_repository.cpp`
  - Create `Server/src/adapters/db/PgMuscleRepository.h` / `.cpp`
  - Create `Server/src/adapters/db/AutoCommitPgMuscleRepository.h` / `.cpp`
  - Create `Server/tests/test_pg_muscle_repository.cpp`, `test_auto_commit_pg_muscle_repository.cpp`
  - Modify `Server/src/app/InjuryService.h`
  - Modify `Server/tests/test_injury_service.cpp`
  - Create `Server/src/http/dto/MuscleDto.h`
  - Create `Server/src/http/controllers/MuscleController.h` / `.cpp`
  - Create `Server/tests/test_muscle_controller.cpp`
  - Modify `Server/src/http/dto/InjuryDto.h`
  - Modify `Server/src/http/controllers/InjuryController.h` / `.cpp`
  - Modify `Server/tests/test_injury_controller.cpp`
  - Modify `Server/src/main.cpp`, `Server/CMakeLists.txt`

---

### Task 1: Migration + `domain::Muscle` + grown `Injury` + `InjuryPolicy` validation

**Files:**
- Create: `Server/migrations/0003_injury_muscle_link.sql`, `Server/src/domain/Muscle.h`
- Modify: `Server/src/domain/Injury.h`, `Server/src/domain/InjuryPolicy.h`, `Server/tests/test_injury_policy.cpp`

**Interfaces:**
- Produces: `domain::Injury` gains `std::string injuryType` + `std::optional<long long> muscleId`; `domain::Muscle { id, name, muscleGroup }`; `requireValidInjury` now also validates `injury_type` and the `muscle_id`/`injury_type` pairing.

- [ ] **Step 1: Write the failing test**

In `Server/tests/test_injury_policy.cpp`, replace the `validInjury()` helper and append new cases:

```cpp
Injury validInjury() {
    Injury i;
    i.userId = 1;
    i.bodyRegion = "lower_back";
    i.severity = "moderate";
    i.injuryType = "other";
    i.active = true;
    return i;
}
```

Append at the end of the file:

```cpp
TEST(InjuryPolicy, BadInjuryType_Throws_FieldIsInjuryType) {
    Injury i = validInjury();
    i.injuryType = "sprain";  // not a real enum value
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "injury_type");
    }
}

TEST(InjuryPolicy, AllFiveInjuryTypes_Accepted) {
    for (const char* t : {"muscle", "bone", "tendon_ligament", "joint", "other"}) {
        Injury i = validInjury();
        i.injuryType = t;
        EXPECT_NO_THROW(requireValidInjury(i)) << t;
    }
}

TEST(InjuryPolicy, MuscleIdWithNonMuscleType_Throws_FieldIsMuscleId) {
    Injury i = validInjury();
    i.injuryType = "bone";
    i.muscleId = 7;
    try {
        requireValidInjury(i);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "muscle_id");
    }
}

TEST(InjuryPolicy, MuscleIdWithMuscleType_DoesNotThrow) {
    Injury i = validInjury();
    i.injuryType = "muscle";
    i.muscleId = 7;
    EXPECT_NO_THROW(requireValidInjury(i));
}

TEST(InjuryPolicy, MuscleTypeWithoutMuscleId_DoesNotThrow) {
    Injury i = validInjury();
    i.injuryType = "muscle";  // muscleId left unset -- never required (E4)
    EXPECT_NO_THROW(requireValidInjury(i));
}
```

- [ ] **Step 2: Run to verify it fails**

Run (from `Server/`, with `C:\msys64\ucrt64\bin` on PATH):

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — compile error, `Injury` has no member `injuryType`/`muscleId`.

- [ ] **Step 3: Grow `domain::Injury`**

Replace `Server/src/domain/Injury.h`:

```cpp
#pragma once

#include <optional>
#include <string>

namespace kinetiq::domain {

// A user's injury. Drives the Phase 3 rule-based safety filter (body_region is
// JOINed against exercise_contraindications) -- body_region is unaffected by
// the muscle link below. Inactive injuries are retained for history but
// excluded from the filter.
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;             // free text in v1 (e.g. "lower_back")
    std::string severity;               // mild|moderate|severe
    std::string injuryType;             // muscle|bone|tendon_ligament|joint|other (required)
    std::optional<long long> muscleId;  // nullable; valid only if injuryType == "muscle"
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;              // ISO timestamp, set by the DB (read-only)
};

}  // namespace kinetiq::domain
```

- [ ] **Step 4: Create `domain::Muscle`**

Create `Server/src/domain/Muscle.h`:

```cpp
#pragma once

#include <string>

namespace kinetiq::domain {

// A catalog muscle (e.g. "Hamstrings"). Read-only from the API's perspective --
// seeded by migrations, never created/edited by a client.
struct Muscle {
    long long id = 0;
    std::string name;
    std::string muscleGroup;
};

}  // namespace kinetiq::domain
```

- [ ] **Step 5: Extend `InjuryPolicy`**

Replace `requireValidInjury` in `Server/src/domain/InjuryPolicy.h`:

```cpp
#pragma once

#include "domain/Injury.h"

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
    const bool typeOk = injury.injuryType == "muscle" || injury.injuryType == "bone" ||
                        injury.injuryType == "tendon_ligament" || injury.injuryType == "joint" ||
                        injury.injuryType == "other";
    if (!typeOk) {
        throw InjuryValidationError(
            "injury_type", "injury_type must be muscle, bone, tendon_ligament, joint, or other");
    }
    // Cross-field rule (E1/E4): a muscle link only makes sense for muscle-type
    // injuries. muscle_id is never required even when injuryType == "muscle".
    if (injury.muscleId.has_value() && injury.injuryType != "muscle") {
        throw InjuryValidationError("muscle_id", "muscle_id is only valid when injury_type is muscle");
    }
}

}  // namespace kinetiq::domain
```

- [ ] **Step 6: Write the migration**

Create `Server/migrations/0003_injury_muscle_link.sql`:

```sql
-- Migration 0003: injury/muscle link.
--
-- Adds injury_type (required) + muscle_id (nullable, valid only for
-- injury_type = 'muscle') to user_injuries, and seeds a common-injury subset
-- of the Muscles catalog. body_region and the Phase 3 safety filter mechanism
-- are unaffected -- this is additive structure. See
-- docs/superpowers/specs/2026-07-25-injury-muscle-link-design.md.

ALTER TABLE user_injuries
    ADD COLUMN injury_type TEXT NOT NULL DEFAULT 'other'
        CHECK (injury_type IN ('muscle','bone','tendon_ligament','joint','other')),
    ADD COLUMN muscle_id BIGINT REFERENCES muscles(id);

-- Enforce the pairing: a muscle link only makes sense for muscle-type injuries.
ALTER TABLE user_injuries
    ADD CONSTRAINT chk_muscle_id_requires_muscle_type
    CHECK (muscle_id IS NULL OR injury_type = 'muscle');

CREATE INDEX idx_user_injuries_muscle_id ON user_injuries(muscle_id);

-- Seed a common-injury subset. Names match 01-data-model.md's documented
-- Phase 2 seed list, so Slice 3's full catalog seed can safely re-run
-- against these (ON CONFLICT DO NOTHING).
INSERT INTO muscles (name, muscle_group) VALUES
    ('Pectorals', 'chest'), ('Anterior Deltoid', 'shoulders'),
    ('Lateral Deltoid', 'shoulders'), ('Posterior Deltoid', 'shoulders'),
    ('Biceps', 'arms'), ('Triceps', 'arms'), ('Lats', 'back'), ('Traps', 'back'),
    ('Rhomboids', 'back'), ('Erector Spinae', 'back'), ('Rectus Abdominis', 'core'),
    ('Obliques', 'core'), ('Glutes', 'legs'), ('Quads', 'legs'),
    ('Hamstrings', 'legs'), ('Calves', 'legs')
ON CONFLICT (name) DO NOTHING;
```

- [ ] **Step 7: Build + run domain tests — verify green**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='InjuryPolicy.*'
```

Expected: **PASS** — 16/16 (11 previous + 5 new).

- [ ] **Step 8: Apply and explicitly verify the migration**

Ensure Postgres is up (`docker compose up -d` from repo root), then run any DB-touching test once
to trigger `ensureTestDatabaseReady()`'s auto-migration (e.g.
`./build/kinetiq_tests.exe --gtest_filter='DbTest.*' 2>&1 | head -20` — watch stderr for
`[testdb] migrating test DB failed:`; if you see that line, the migration has a bug — fix it before
continuing, since a broken migration otherwise just silently skips every DB test rather than
failing loudly).

Then verify directly against the test DB:

```
docker exec kinetiq-postgres psql -U kinetiq -d kinetiq_test -c "\d user_injuries" -c "SELECT count(*) FROM muscles;" -c "SELECT name, muscle_group FROM muscles ORDER BY id LIMIT 3;"
```

Expected: `user_injuries` shows `injury_type` and `muscle_id` columns; muscle count = `16`; first
three rows are `Pectorals`/`Anterior Deltoid`/`Lateral Deltoid`.

- [ ] **Step 9: Commit**

```
git add Server/migrations/0003_injury_muscle_link.sql Server/src/domain/Injury.h Server/src/domain/Muscle.h Server/src/domain/InjuryPolicy.h Server/tests/test_injury_policy.cpp
git commit -m "feat(server): injury_type + muscle_id domain model + migration (TDD)"
```

---

### Task 2: Persist `injury_type`/`muscle_id` through the injury repositories

**Files:**
- Modify: `Server/src/adapters/db/PgInjuryRepository.cpp`
- Modify: `Server/tests/test_injury_repository.cpp`, `test_pg_injury_repository.cpp`, `test_auto_commit_pg_injury_repository.cpp`

**Interfaces:**
- Consumes: grown `domain::Injury` (Task 1).
- Produces: `PgInjuryRepository`/`AutoCommitPgInjuryRepository` round-trip the two new fields. `InMemoryInjuryRepository` needs **no code change** (stores `Injury` by value) — only its tests gain fixture updates + a guard test, same pattern as Stage A2's fake round-trip.

- [ ] **Step 1: Write the failing tests**

In `Server/tests/test_injury_repository.cpp`, replace the `make()` helper:

```cpp
Injury make(long long userId, const std::string& region, const std::string& severity) {
    Injury i;
    i.userId = userId;
    i.bodyRegion = region;
    i.severity = severity;
    i.injuryType = "other";
    return i;
}
```

Append at the end of the file:

```cpp
// injury_type/muscle_id round-trip with zero fake-code changes (stores the
// whole Injury by value) -- guard it, same pattern as the profile fake test.
TEST(InMemoryInjuryRepositoryTest, Create_PersistsInjuryTypeAndMuscleId) {
    InMemoryInjuryRepository repo;
    Injury i = make(1, "hamstring", "moderate");
    i.injuryType = "muscle";
    i.muscleId = 15;
    const Injury saved = repo.create(i);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->injuryType, "muscle");
    EXPECT_EQ(found->muscleId, 15);
}
```

In `Server/tests/test_pg_injury_repository.cpp`, replace the `make()` helper the same way, and add
a helper + test at the end of the file:

```cpp
Injury make(long long userId, const std::string& region, const std::string& severity) {
    Injury i;
    i.userId = userId;
    i.bodyRegion = region;
    i.severity = severity;
    i.injuryType = "other";
    return i;
}
```

```cpp
namespace {
long long findMuscleIdByName(pqxx::work& tx, const std::string& name) {
    const auto result = tx.exec("SELECT id FROM muscles WHERE name = $1", pqxx::params{name});
    return result[0][0].as<long long>();
}
}  // namespace

TEST_F(DbTest, PgInjuryRepository_Create_PersistsInjuryTypeAndMuscleId) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    const long long userId = seedUser(tx(), "pg-injury-muscle@example.com");
    const long long muscleId = findMuscleIdByName(tx(), "Hamstrings");
    PgInjuryRepository repo(tx());

    Injury injury = make(userId, "hamstring", "moderate");
    injury.injuryType = "muscle";
    injury.muscleId = muscleId;
    const Injury saved = repo.create(injury);

    EXPECT_EQ(saved.injuryType, "muscle");
    EXPECT_EQ(saved.muscleId, muscleId);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->injuryType, "muscle");
    EXPECT_EQ(found->muscleId, muscleId);
}
```

(Note: the second `namespace { ... }` block adds `findMuscleIdByName` alongside the file's existing
anonymous namespace containing `seedUser`/`make` — keep them in the same or a second anonymous
namespace, either compiles fine.)

In `Server/tests/test_auto_commit_pg_injury_repository.cpp`, in
`CreateThenList_AcrossSeparateConnections`, add one line after the existing `i.severity = ...;`:

```cpp
    i.injuryType = "other";
```

- [ ] **Step 2: Run to verify it fails**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='*InjuryRepository*'
```

Expected: the new `InMemoryInjuryRepositoryTest.Create_PersistsInjuryTypeAndMuscleId` **PASSES**
already (fake stores by value — no red here, same as Stage A2's fake round-trip). The new
`PgInjuryRepository_Create_PersistsInjuryTypeAndMuscleId` **FAILS**: `saved.injuryType` comes back
empty (`""`) because `PgInjuryRepository`'s `INSERT`/`SELECT` don't touch the new columns yet.

- [ ] **Step 3: Extend `PgInjuryRepository.cpp`**

Replace the anonymous-namespace contents and the `create`/`update` bodies in
`Server/src/adapters/db/PgInjuryRepository.cpp`:

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
    injury.injuryType = row["injury_type"].is_null() ? "" : row["injury_type"].as<std::string>();
    if (!row["muscle_id"].is_null()) injury.muscleId = row["muscle_id"].as<long long>();
    injury.active = row["active"].as<bool>();
    if (!row["notes"].is_null()) injury.notes = row["notes"].as<std::string>();
    injury.createdAt = row["created_at"].as<std::string>();
    return injury;
}

constexpr const char* kColumns =
    "id, user_id, body_region, severity, injury_type, muscle_id, active, notes, created_at";

}  // namespace

kinetiq::domain::Injury PgInjuryRepository::create(const kinetiq::domain::Injury& injury) {
    const auto result = tx_.exec(
        "INSERT INTO user_injuries (user_id, body_region, severity, injury_type, muscle_id, "
        "active, notes) VALUES ($1, $2, $3, $4, $5, $6, $7) "
        "RETURNING id, user_id, body_region, severity, injury_type, muscle_id, active, notes, "
        "created_at",
        pqxx::params{injury.userId, injury.bodyRegion, injury.severity, injury.injuryType,
                     injury.muscleId, injury.active, injury.notes});
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
        "UPDATE user_injuries SET body_region = $1, severity = $2, injury_type = $3, "
        "muscle_id = $4, active = $5, notes = $6 WHERE id = $7",
        pqxx::params{injury.bodyRegion, injury.severity, injury.injuryType, injury.muscleId,
                     injury.active, injury.notes, injury.id});
}

void PgInjuryRepository::deleteById(long long id) {
    tx_.exec("DELETE FROM user_injuries WHERE id = $1", pqxx::params{id});
}

}  // namespace kinetiq::db
```

- [ ] **Step 4: Build + run — verify green**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='*InjuryRepository*'
```

Expected: **PASS** — all injury-repository tests, including the two new ones.

- [ ] **Step 5: Commit**

```
git add Server/src/adapters/db/PgInjuryRepository.cpp Server/tests/test_injury_repository.cpp Server/tests/test_pg_injury_repository.cpp Server/tests/test_auto_commit_pg_injury_repository.cpp
git commit -m "feat(server): persist injury_type/muscle_id through PgInjuryRepository (TDD)"
```

---

### Task 3: `IMuscleRepository` port + in-memory fake

**Files:**
- Create: `Server/src/ports/IMuscleRepository.h`, `Server/tests/support/InMemoryMuscleRepository.h`
- Test: `Server/tests/test_muscle_repository.cpp`
- Modify: `Server/CMakeLists.txt` (add test source)

**Interfaces:**
- Produces: `ports::IMuscleRepository { list() -> vector<Muscle>; findById(long long) -> optional<Muscle>; }`; `testsupport::InMemoryMuscleRepository` implementing it, pre-seeded with `{1,"Hamstrings","legs"}`, `{2,"Quads","legs"}`, `{3,"Biceps","arms"}` (fixed ids so later tasks' tests can reference `muscleId = 1` deterministically).

- [ ] **Step 1: Write the failing test**

Create `Server/tests/test_muscle_repository.cpp`:

```cpp
// Injury/muscle link: IMuscleRepository, exercised against the in-memory fake.
// The real adapter (PgMuscleRepository) must satisfy the same contract.

#include <gtest/gtest.h>

#include "support/InMemoryMuscleRepository.h"

using kinetiq::testsupport::InMemoryMuscleRepository;

TEST(InMemoryMuscleRepositoryTest, List_ReturnsSeededMuscles) {
    InMemoryMuscleRepository repo;
    const auto muscles = repo.list();
    EXPECT_GE(muscles.size(), 1u);
}

TEST(InMemoryMuscleRepositoryTest, FindById_KnownId_ReturnsMuscle) {
    InMemoryMuscleRepository repo;
    const auto found = repo.findById(1);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "Hamstrings");
}

TEST(InMemoryMuscleRepositoryTest, FindById_UnknownId_ReturnsNullopt) {
    InMemoryMuscleRepository repo;
    EXPECT_FALSE(repo.findById(999).has_value());
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt`, `kinetiq_tests` source list, add after `tests/test_injury_service.cpp`:

```cmake
        tests/test_muscle_repository.cpp
```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — `fatal error: support/InMemoryMuscleRepository.h: No such file or directory`.

- [ ] **Step 3: Write the port**

Create `Server/src/ports/IMuscleRepository.h`:

```cpp
#pragma once

#include "domain/Muscle.h"

#include <optional>
#include <vector>

namespace kinetiq::ports {

// Read-only repository port for the Muscles catalog. No client ever creates or
// edits muscles via the API -- list/lookup are the only operations needed.
class IMuscleRepository {
public:
    virtual ~IMuscleRepository() = default;

    virtual std::vector<domain::Muscle> list() = 0;
    virtual std::optional<domain::Muscle> findById(long long id) = 0;
};

}  // namespace kinetiq::ports
```

- [ ] **Step 4: Write the fake**

Create `Server/tests/support/InMemoryMuscleRepository.h`:

```cpp
#pragma once

#include "ports/IMuscleRepository.h"

#include <optional>
#include <vector>

namespace kinetiq::testsupport {

// Hand-written in-memory fake for kinetiq::ports::IMuscleRepository, pre-seeded
// with a small fixed set so tests have deterministic ids to reference.
class InMemoryMuscleRepository : public kinetiq::ports::IMuscleRepository {
public:
    InMemoryMuscleRepository() {
        muscles_.push_back({1, "Hamstrings", "legs"});
        muscles_.push_back({2, "Quads", "legs"});
        muscles_.push_back({3, "Biceps", "arms"});
    }

    std::vector<kinetiq::domain::Muscle> list() override { return muscles_; }

    std::optional<kinetiq::domain::Muscle> findById(long long id) override {
        for (const auto& m : muscles_) {
            if (m.id == id) return m;
        }
        return std::nullopt;
    }

private:
    std::vector<kinetiq::domain::Muscle> muscles_;
};

}  // namespace kinetiq::testsupport
```

- [ ] **Step 5: Build + run — verify green**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='InMemoryMuscleRepositoryTest.*'
```

Expected: **PASS** — 3/3.

- [ ] **Step 6: Commit**

```
git add Server/src/ports/IMuscleRepository.h Server/tests/support/InMemoryMuscleRepository.h Server/tests/test_muscle_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): IMuscleRepository port + in-memory fake (TDD)"
```

---

### Task 4: `PgMuscleRepository` + `AutoCommitPgMuscleRepository`

**Files:**
- Create: `Server/src/adapters/db/PgMuscleRepository.h` / `.cpp`, `Server/src/adapters/db/AutoCommitPgMuscleRepository.h` / `.cpp`
- Test: `Server/tests/test_pg_muscle_repository.cpp`, `Server/tests/test_auto_commit_pg_muscle_repository.cpp`
- Modify: `Server/CMakeLists.txt` (both `.cpp` into `kinetiq_server` + `kinetiq_tests`; two test sources into `kinetiq_tests`)

**Interfaces:**
- Consumes: `ports::IMuscleRepository`, `domain::Muscle`. `PgMuscleRepository(pqxx::work&)`; `AutoCommitPgMuscleRepository(std::string connString)`.
- Produces: real Postgres reads of the seeded muscle catalog (from Task 1's migration).

- [ ] **Step 1: Write the failing tests**

Create `Server/tests/test_pg_muscle_repository.cpp`:

```cpp
// Injury/muscle link: PgMuscleRepository against real Postgres (read-only,
// reads the catalog seeded by 0003_injury_muscle_link.sql).

#include "support/TestDb.h"

#include <algorithm>
#include <gtest/gtest.h>

#include "adapters/db/PgMuscleRepository.h"

using kinetiq::db::PgMuscleRepository;

TEST_F(DbTest, PgMuscleRepository_List_ReturnsSeededMuscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgMuscleRepository repo(tx());
    const auto muscles = repo.list();
    EXPECT_GE(muscles.size(), 16u);
    const bool hasHamstrings = std::any_of(
        muscles.begin(), muscles.end(), [](const auto& m) { return m.name == "Hamstrings"; });
    EXPECT_TRUE(hasHamstrings);
}

TEST_F(DbTest, PgMuscleRepository_FindById_ReturnsMuscle) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgMuscleRepository repo(tx());
    const auto all = repo.list();
    ASSERT_FALSE(all.empty());
    const auto found = repo.findById(all[0].id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, all[0].name);
}

TEST_F(DbTest, PgMuscleRepository_FindById_UnknownId_ReturnsNullopt) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgMuscleRepository repo(tx());
    EXPECT_FALSE(repo.findById(999999999).has_value());
}
```

Create `Server/tests/test_auto_commit_pg_muscle_repository.cpp`:

```cpp
// Injury/muscle link: AutoCommitPgMuscleRepository -- the repo wired into
// main.cpp. Read-only, so no cleanup is needed (never writes).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgMuscleRepository.h"

using kinetiq::db::AutoCommitPgMuscleRepository;

TEST(AutoCommitPgMuscleRepositoryTest, List_ReturnsSeededMuscles) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }

    AutoCommitPgMuscleRepository repo(kinetiq::testing::testConnString());
    const auto muscles = repo.list();
    EXPECT_GE(muscles.size(), 16u);
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt`:
- `kinetiq_server` sources — after `src/adapters/db/AutoCommitPgInjuryRepository.cpp`:
  ```cmake
      src/adapters/db/PgMuscleRepository.cpp
      src/adapters/db/AutoCommitPgMuscleRepository.cpp
  ```
- `kinetiq_tests` sources — after its `src/adapters/db/AutoCommitPgInjuryRepository.cpp` line:
  ```cmake
        src/adapters/db/PgMuscleRepository.cpp
        src/adapters/db/AutoCommitPgMuscleRepository.cpp
  ```
- `kinetiq_tests` — after `tests/test_auto_commit_pg_injury_repository.cpp`:
  ```cmake
        tests/test_pg_muscle_repository.cpp
        tests/test_auto_commit_pg_muscle_repository.cpp
  ```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — CMake `Cannot find source file: ...PgMuscleRepository.cpp`.

- [ ] **Step 3: Write `PgMuscleRepository.h`**

Create `Server/src/adapters/db/PgMuscleRepository.h`:

```cpp
#pragma once

#include "domain/Muscle.h"
#include "ports/IMuscleRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed IMuscleRepository (read-only). Operates within a transaction
// it is handed, same convention as PgInjuryRepository/PgUserRepository.
class PgMuscleRepository : public kinetiq::ports::IMuscleRepository {
public:
    explicit PgMuscleRepository(pqxx::work& tx) : tx_(tx) {}

    std::vector<kinetiq::domain::Muscle> list() override;
    std::optional<kinetiq::domain::Muscle> findById(long long id) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
```

- [ ] **Step 4: Write `PgMuscleRepository.cpp`**

Create `Server/src/adapters/db/PgMuscleRepository.cpp`:

```cpp
#include "adapters/db/PgMuscleRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::Muscle rowToMuscle(const pqxx::row& row) {
    kinetiq::domain::Muscle muscle;
    muscle.id = row["id"].as<long long>();
    muscle.name = row["name"].as<std::string>();
    muscle.muscleGroup = row["muscle_group"].as<std::string>();
    return muscle;
}

}  // namespace

std::vector<kinetiq::domain::Muscle> PgMuscleRepository::list() {
    const auto result = tx_.exec("SELECT id, name, muscle_group FROM muscles ORDER BY id");
    std::vector<kinetiq::domain::Muscle> out;
    out.reserve(result.size());
    for (const auto& row : result) out.push_back(rowToMuscle(row));
    return out;
}

std::optional<kinetiq::domain::Muscle> PgMuscleRepository::findById(long long id) {
    const auto result =
        tx_.exec("SELECT id, name, muscle_group FROM muscles WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToMuscle(result[0]);
}

}  // namespace kinetiq::db
```

- [ ] **Step 5: Write the `AutoCommit` wrapper**

Create `Server/src/adapters/db/AutoCommitPgMuscleRepository.h`:

```cpp
#pragma once

#include "ports/IMuscleRepository.h"

#include <string>

namespace kinetiq::db {

// Production IMuscleRepository: opens a fresh connection + transaction per
// call, delegates to PgMuscleRepository, commits. Same rationale as the other
// AutoCommit* repos (a Crow route captures its deps for the server's lifetime;
// kept for consistency even though muscles are read-only).
class AutoCommitPgMuscleRepository : public kinetiq::ports::IMuscleRepository {
public:
    explicit AutoCommitPgMuscleRepository(std::string connString)
        : connString_(std::move(connString)) {}

    std::vector<kinetiq::domain::Muscle> list() override;
    std::optional<kinetiq::domain::Muscle> findById(long long id) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
```

Create `Server/src/adapters/db/AutoCommitPgMuscleRepository.cpp`:

```cpp
#include "adapters/db/AutoCommitPgMuscleRepository.h"

#include "adapters/db/PgMuscleRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

std::vector<kinetiq::domain::Muscle> AutoCommitPgMuscleRepository::list() {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgMuscleRepository repo(tx);
    auto result = repo.list();
    tx.commit();
    return result;
}

std::optional<kinetiq::domain::Muscle> AutoCommitPgMuscleRepository::findById(long long id) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgMuscleRepository repo(tx);
    auto result = repo.findById(id);
    tx.commit();
    return result;
}

}  // namespace kinetiq::db
```

- [ ] **Step 6: Build + run — verify green (Postgres up)**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='*MuscleRepository*'
```

Expected: **PASS** — 3 fake + 3 `PgMuscleRepository` + 1 auto-commit test.

- [ ] **Step 7: Commit**

```
git add Server/src/adapters/db/PgMuscleRepository.h Server/src/adapters/db/PgMuscleRepository.cpp Server/src/adapters/db/AutoCommitPgMuscleRepository.h Server/src/adapters/db/AutoCommitPgMuscleRepository.cpp Server/tests/test_pg_muscle_repository.cpp Server/tests/test_auto_commit_pg_muscle_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): PgMuscleRepository + AutoCommit wrapper (TDD)"
```

---

### Task 5: `InjuryService` — muscle validation + update cascade

**Files:**
- Modify: `Server/src/app/InjuryService.h`, `Server/tests/test_injury_service.cpp`

**Interfaces:**
- Consumes: `ports::IMuscleRepository` (Task 3).
- Produces: `InjuryService(IInjuryRepository&, IMuscleRepository&)` (constructor signature changes — **breaking** for every existing caller); `AddInjuryRequest` gains `std::string injuryType` + `std::optional<long long> muscleId`; `UpdateInjuryFields` gains `std::optional<std::string> injuryType` + `std::optional<long long> muscleId`.

- [ ] **Step 1: Write the failing test**

Replace `Server/tests/test_injury_service.cpp` in full:

```cpp
// Injury/muscle link: InjuryService -- CRUD over IInjuryRepository with an
// ownership check, plus muscle validation and the injury_type/muscle_id
// cross-field cascade.

#include <gtest/gtest.h>

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "support/InMemoryInjuryRepository.h"
#include "support/InMemoryMuscleRepository.h"

using kinetiq::app::AddInjuryRequest;
using kinetiq::app::InjuryNotFoundError;
using kinetiq::app::InjuryService;
using kinetiq::app::UpdateInjuryFields;
using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::testsupport::InMemoryInjuryRepository;
using kinetiq::testsupport::InMemoryMuscleRepository;

namespace {

AddInjuryRequest addReq(long long userId, const std::string& region, const std::string& severity) {
    AddInjuryRequest r;
    r.userId = userId;
    r.bodyRegion = region;
    r.severity = severity;
    r.injuryType = "other";
    return r;
}

}  // namespace

TEST(InjuryService, Add_ValidInjury_CreatesAndAppearsInList) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);

    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_GT(created.id, 0);

    const auto list = svc.list(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
}

TEST(InjuryService, Add_BadSeverity_Throws) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    EXPECT_THROW(svc.add(addReq(1, "lower_back", "critical")), InjuryValidationError);
}

TEST(InjuryService, Add_BadInjuryType_Throws) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    auto req = addReq(1, "lower_back", "moderate");
    req.injuryType = "sprain";
    EXPECT_THROW(svc.add(req), InjuryValidationError);
}

TEST(InjuryService, Add_MuscleTypeWithValidMuscleId_Succeeds) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;  // pre-seeded with id 1 = "Hamstrings"
    InjuryService svc(repo, muscles);

    auto req = addReq(1, "hamstring", "moderate");
    req.injuryType = "muscle";
    req.muscleId = 1;
    const Injury created = svc.add(req);
    EXPECT_EQ(created.injuryType, "muscle");
    EXPECT_EQ(created.muscleId, 1);
}

TEST(InjuryService, Add_MuscleTypeWithUnknownMuscleId_ThrowsValidationError) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);

    auto req = addReq(1, "hamstring", "moderate");
    req.injuryType = "muscle";
    req.muscleId = 999999;  // doesn't exist
    try {
        svc.add(req);
        FAIL() << "expected InjuryValidationError";
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), "muscle_id");
    }
}

TEST(InjuryService, Add_NonMuscleTypeWithMuscleId_Throws) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);

    auto req = addReq(1, "shin", "mild");
    req.injuryType = "bone";
    req.muscleId = 1;  // inconsistent with a non-muscle type
    EXPECT_THROW(svc.add(req), InjuryValidationError);
}

TEST(InjuryService, Update_OwnInjury_ChangesFields) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    const Injury updated = svc.update(1, created.id, fields);
    EXPECT_FALSE(updated.active);
}

TEST(InjuryService, Update_ChangingTypeAwayFromMuscle_ClearsMuscleId) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);

    auto req = addReq(1, "hamstring", "moderate");
    req.injuryType = "muscle";
    req.muscleId = 1;
    const Injury created = svc.add(req);
    ASSERT_EQ(created.muscleId, 1);

    UpdateInjuryFields fields;
    fields.injuryType = "bone";  // no longer muscle-related
    const Injury updated = svc.update(1, created.id, fields);
    EXPECT_EQ(updated.injuryType, "bone");
    EXPECT_FALSE(updated.muscleId.has_value());  // auto-cleared, not left dangling
}

TEST(InjuryService, Update_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));

    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_THROW(svc.update(2, created.id, fields), InjuryNotFoundError);  // user 2 != owner 1
}

TEST(InjuryService, Remove_OwnInjury_Deletes) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    svc.remove(1, created.id);
    EXPECT_TRUE(svc.list(1).empty());
}

TEST(InjuryService, Remove_OtherUsersInjury_ThrowsNotFound) {
    InMemoryInjuryRepository repo;
    InMemoryMuscleRepository muscles;
    InjuryService svc(repo, muscles);
    const Injury created = svc.add(addReq(1, "lower_back", "moderate"));
    EXPECT_THROW(svc.remove(2, created.id), InjuryNotFoundError);
    EXPECT_EQ(svc.list(1).size(), 1u);  // still there
}
```

- [ ] **Step 2: Run to verify it fails**

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — compile error, `InjuryService`'s constructor doesn't accept a second argument;
`AddInjuryRequest`/`UpdateInjuryFields` have no `injuryType`/`muscleId` members.

- [ ] **Step 3: Rewrite `InjuryService.h`**

Replace `Server/src/app/InjuryService.h` in full:

```cpp
#pragma once

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"
#include "ports/IInjuryRepository.h"
#include "ports/IMuscleRepository.h"

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
    std::string injuryType;             // muscle|bone|tendon_ligament|joint|other (required)
    std::optional<long long> muscleId;  // valid only if injuryType == "muscle"
    std::optional<std::string> notes;
};

// Partial update: present fields change, absent fields are left unchanged.
struct UpdateInjuryFields {
    std::optional<std::string> bodyRegion;
    std::optional<std::string> severity;
    std::optional<std::string> injuryType;
    std::optional<long long> muscleId;
    std::optional<std::string> notes;
    std::optional<bool> active;
};

// Cohesive CRUD service for a user's injuries. One class (not four use-case
// classes) because the four operations share the ownership check (D5).
class InjuryService {
public:
    InjuryService(kinetiq::ports::IInjuryRepository& injuries,
                 kinetiq::ports::IMuscleRepository& muscles)
        : injuries_(injuries), muscles_(muscles) {}

    kinetiq::domain::Injury add(const AddInjuryRequest& req) {
        kinetiq::domain::Injury injury;
        injury.userId = req.userId;
        injury.bodyRegion = req.bodyRegion;
        injury.severity = req.severity;
        injury.injuryType = req.injuryType;
        injury.muscleId = req.muscleId;
        injury.notes = req.notes;
        injury.active = true;
        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        requireMuscleExistsIfSet(injury);
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
        if (fields.injuryType) injury.injuryType = *fields.injuryType;
        if (fields.muscleId) injury.muscleId = fields.muscleId;
        if (fields.notes) injury.notes = fields.notes;
        if (fields.active) injury.active = *fields.active;

        // Cascade (E5): changing type away from "muscle" invalidates any
        // muscle link -- clear it here rather than let the DB CHECK
        // constraint reject the row. Applied after the fields above so it
        // wins regardless of payload order.
        if (injury.injuryType != "muscle") {
            injury.muscleId = std::nullopt;
        }

        kinetiq::domain::requireValidInjury(injury);  // guard before persist
        requireMuscleExistsIfSet(injury);
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

    // If the injury references a muscle, confirm it actually exists --
    // otherwise a bad id would surface as a raw FK-violation 500 instead of a
    // clean 422 (E4).
    void requireMuscleExistsIfSet(const kinetiq::domain::Injury& injury) {
        if (!injury.muscleId.has_value()) return;
        if (!muscles_.findById(*injury.muscleId).has_value()) {
            throw kinetiq::domain::InjuryValidationError("muscle_id", "muscle not found");
        }
    }

    kinetiq::ports::IInjuryRepository& injuries_;
    kinetiq::ports::IMuscleRepository& muscles_;
};

}  // namespace kinetiq::app
```

- [ ] **Step 4: Build + run — verify green**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='InjuryService.*'
```

Expected: **PASS** — 11/11 (6 previous + 5 new).

- [ ] **Step 5: Commit**

```
git add Server/src/app/InjuryService.h Server/tests/test_injury_service.cpp
git commit -m "feat(server): InjuryService muscle validation + update cascade (TDD)"
```

---

### Task 6: `MuscleDto` + `MuscleController` (`GET /muscles`, `GET /muscles/{id}`)

**Files:**
- Create: `Server/src/http/dto/MuscleDto.h`, `Server/src/http/controllers/MuscleController.h` / `.cpp`
- Test: `Server/tests/test_muscle_controller.cpp`
- Modify: `Server/CMakeLists.txt` (`MuscleController.cpp` into `kinetiq_server` + `kinetiq_tests`; test into `kinetiq_tests`)

**Interfaces:**
- Consumes: `ports::IMuscleRepository`, `http::authenticate`, `dto::errorResponse` (AuthDto.h).
- Produces: `dto::muscleToJson`, `dto::muscleListToJson`; `http::registerMuscleRoutes(crow::SimpleApp&, ports::IMuscleRepository&, ports::ITokenService&)`.

- [ ] **Step 1: Write the failing test**

Create `Server/tests/test_muscle_controller.cpp`:

```cpp
// Injury/muscle link: HTTP layer for GET /muscles + GET /muscles/{id}.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/MuscleController.h"
#include "support/InMemoryMuscleRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryMuscleRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct MuscleApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryMuscleRepository muscles;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    MuscleApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerMuscleRoutes(app, muscles, tokens);
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

TEST(MuscleControllerTest, List_ValidToken_Returns200WithSeededMuscles) {
    MuscleApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "musc-a@example.com");

    const auto r = client.get("/api/v1/muscles", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    ASSERT_GE(body.size(), 1u);
}

TEST(MuscleControllerTest, List_MissingToken_Returns401) {
    MuscleApi api;
    kinetiq::testing::HttpClient client(api.app);
    EXPECT_EQ(client.get("/api/v1/muscles").code, 401);
}

TEST(MuscleControllerTest, GetById_KnownId_Returns200) {
    MuscleApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "musc-b@example.com");

    const auto r = client.get("/api/v1/muscles/1", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["name"].s()), "Hamstrings");
}

TEST(MuscleControllerTest, GetById_UnknownId_Returns404) {
    MuscleApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "musc-c@example.com");

    const auto r = client.get("/api/v1/muscles/999999", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 404);
}
```

- [ ] **Step 2: Register in CMake, verify red**

In `Server/CMakeLists.txt`:
- `kinetiq_server` sources — after `src/http/controllers/InjuryController.cpp`:
  ```cmake
      src/http/controllers/MuscleController.cpp
  ```
- `kinetiq_tests` sources — after its `src/http/controllers/InjuryController.cpp` line:
  ```cmake
        src/http/controllers/MuscleController.cpp
  ```
- `kinetiq_tests` — after `tests/test_injury_controller.cpp`:
  ```cmake
        tests/test_muscle_controller.cpp
  ```

Run: `cmake --build --preset default --target kinetiq_tests`
Expected: **FAIL** — CMake can't find `MuscleController.cpp`.

- [ ] **Step 3: Write `MuscleDto.h`**

Create `Server/src/http/dto/MuscleDto.h`:

```cpp
#pragma once

#include "domain/Muscle.h"

#include "crow_all.h"

#include <vector>

namespace kinetiq::http::dto {

inline crow::json::wvalue muscleToJson(const kinetiq::domain::Muscle& muscle) {
    crow::json::wvalue json;
    json["id"] = muscle.id;
    json["name"] = muscle.name;
    json["muscle_group"] = muscle.muscleGroup;
    return json;
}

// Uses the wvalue(list) constructor (list = std::vector<wvalue>) so an empty
// list serializes as [] (not null) -- same technique as InjuryDto.
inline crow::json::wvalue muscleListToJson(const std::vector<kinetiq::domain::Muscle>& muscles) {
    std::vector<crow::json::wvalue> items;
    items.reserve(muscles.size());
    for (const auto& muscle : muscles) items.push_back(muscleToJson(muscle));
    return crow::json::wvalue(items);
}

}  // namespace kinetiq::http::dto
```

- [ ] **Step 4: Write `MuscleController.h`**

Create `Server/src/http/controllers/MuscleController.h`:

```cpp
#pragma once

#include "ports/IMuscleRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers GET /api/v1/muscles + GET /api/v1/muscles/{id} on `app`. Read-only,
// 🔒 like the rest of the API. Free function (matches the other controllers).
void registerMuscleRoutes(crow::SimpleApp& app, kinetiq::ports::IMuscleRepository& muscles,
                          kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
```

- [ ] **Step 5: Write `MuscleController.cpp`**

Create `Server/src/http/controllers/MuscleController.cpp`:

```cpp
#include "http/controllers/MuscleController.h"

#include "http/dto/AuthDto.h"    // dto::errorResponse
#include "http/dto/MuscleDto.h"
#include "http/middleware/Authenticator.h"

#include <cstdint>

namespace kinetiq::http {

void registerMuscleRoutes(crow::SimpleApp& app, kinetiq::ports::IMuscleRepository& muscles,
                          kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/muscles")
        ([&muscles, &tokens](const crow::request& req) -> crow::response {
            const auto currentUser = authenticate(req, tokens);
            if (!currentUser.has_value()) {
                return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                          "Missing, invalid, or expired token.");
            }
            return crow::response(200, dto::muscleListToJson(muscles.list()));
        });

    CROW_ROUTE(app, "/api/v1/muscles/<int>")
        ([&muscles, &tokens](const crow::request& req, std::int64_t id) -> crow::response {
            const auto currentUser = authenticate(req, tokens);
            if (!currentUser.has_value()) {
                return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                          "Missing, invalid, or expired token.");
            }
            const auto found = muscles.findById(id);
            if (!found.has_value()) {
                return dto::errorResponse(404, "not_found", "Not Found", "Muscle not found.");
            }
            return crow::response(200, dto::muscleToJson(*found));
        });
}

}  // namespace kinetiq::http
```

- [ ] **Step 6: Build + run — verify green**

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='MuscleControllerTest.*'
```

Expected: **PASS** — 4/4.

- [ ] **Step 7: Commit**

```
git add Server/src/http/dto/MuscleDto.h Server/src/http/controllers/MuscleController.h Server/src/http/controllers/MuscleController.cpp Server/tests/test_muscle_controller.cpp Server/CMakeLists.txt
git commit -m "feat(server): GET /muscles + GET /muscles/{id} HTTP layer (TDD)"
```

---

### Task 7: Extend `InjuryDto` + `InjuryController` for `injury_type`/`muscle_id`

**Files:**
- Modify: `Server/src/http/dto/InjuryDto.h`, `Server/src/http/controllers/InjuryController.h` / `.cpp`, `Server/tests/test_injury_controller.cpp`

**Interfaces:**
- Consumes: `IMuscleRepository` (Task 3), grown `InjuryService` (Task 5).
- Produces: `registerInjuryRoutes` signature changes to `(app, IInjuryRepository&, IMuscleRepository&, ITokenService&)` — **breaking** for `main.cpp` (Task 8).

- [ ] **Step 1: Write the failing test**

Replace `Server/tests/test_injury_controller.cpp` in full:

```cpp
// Injury/muscle link: HTTP layer for /users/me/injuries CRUD, incl.
// injury_type + muscle_id. Drives real routing via HttpClient. Registers
// auth + injury routes; two users prove ownership isolation.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/InjuryController.h"
#include "support/InMemoryInjuryRepository.h"
#include "support/InMemoryMuscleRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryInjuryRepository;
using kinetiq::testsupport::InMemoryMuscleRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct InjuryApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryInjuryRepository injuries;
    InMemoryMuscleRepository muscles;  // pre-seeded: id 1 = "Hamstrings"
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    InjuryApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerInjuryRoutes(app, injuries, muscles, tokens);
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

    const auto created = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
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

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"lower_back","severity":"critical","injury_type":"other"})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_BadInjuryType_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-type@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"shin","severity":"mild","injury_type":"sprain"})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_MuscleTypeWithValidMuscleId_Returns201) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-musc-ok@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"hamstring","severity":"moderate","injury_type":"muscle","muscle_id":1})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 201);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(body["muscle_id"].i(), 1);
}

TEST(InjuryControllerTest, Create_MuscleTypeWithUnknownMuscleId_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-musc-bad@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"hamstring","severity":"moderate","injury_type":"muscle","muscle_id":999999})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_NonMuscleTypeWithMuscleId_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-musc-mismatch@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"shin","severity":"mild","injury_type":"bone","muscle_id":1})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_Unauthenticated_Returns401) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"lower_back","severity":"mild","injury_type":"other"})");
    EXPECT_EQ(r.code, 401);
}

TEST(InjuryControllerTest, Patch_OwnInjury_Returns200) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-c@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"active":false})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
}

TEST(InjuryControllerTest, Patch_ChangingTypeAwayFromMuscle_ClearsMuscleId) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-cascade@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"hamstring","severity":"moderate","injury_type":"muscle","muscle_id":1})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"injury_type":"bone"})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(body["muscle_id"].t(), crow::json::type::Null);
}

TEST(InjuryControllerTest, Patch_OtherUsersInjury_Returns404) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string tokenA = registerAndGetToken(client, "inj-owner@example.com");
    const std::string tokenB = registerAndGetToken(client, "inj-other@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
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
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
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
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + tokenA}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Delete,
                               "/api/v1/users/me/injuries/" + std::to_string(id), "",
                               {{"Authorization", "Bearer " + tokenB}});
    EXPECT_EQ(r.code, 404);
}
```

- [ ] **Step 2: Run to verify it fails**

```
cmake --build --preset default --target kinetiq_tests
```

Expected: **FAIL** — compile error, `registerInjuryRoutes` doesn't accept a `muscles` argument.

- [ ] **Step 3: Extend `InjuryDto.h`**

Replace `Server/src/http/dto/InjuryDto.h` in full:

```cpp
#pragma once

#include "app/InjuryService.h"
#include "domain/Injury.h"

#include "crow_all.h"

#include <string>
#include <vector>

namespace kinetiq::http::dto {

// Parses POST body into an AddInjuryRequest, pairing it with the authenticated
// user id (never from the body).
inline kinetiq::app::AddInjuryRequest parseAddInjury(const crow::json::rvalue& body,
                                                     long long userId) {
    kinetiq::app::AddInjuryRequest req;
    req.userId = userId;
    req.bodyRegion = std::string(body["body_region"].s());
    req.severity = std::string(body["severity"].s());
    req.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_id")) req.muscleId = body["muscle_id"].i();
    if (body.has("notes")) req.notes = std::string(body["notes"].s());
    return req;
}

// Parses PATCH body into UpdateInjuryFields -- only present keys are set.
inline kinetiq::app::UpdateInjuryFields parseUpdateInjury(const crow::json::rvalue& body) {
    kinetiq::app::UpdateInjuryFields fields;
    if (body.has("body_region")) fields.bodyRegion = std::string(body["body_region"].s());
    if (body.has("severity")) fields.severity = std::string(body["severity"].s());
    if (body.has("injury_type")) fields.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_id")) fields.muscleId = body["muscle_id"].i();
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
    json["injury_type"] = injury.injuryType;
    json["muscle_id"] =
        injury.muscleId ? crow::json::wvalue(*injury.muscleId) : crow::json::wvalue(nullptr);
    json["active"] = injury.active;
    json["notes"] = injury.notes ? crow::json::wvalue(*injury.notes) : crow::json::wvalue(nullptr);
    json["created_at"] = injury.createdAt;
    return json;
}

// Builds a JSON array. Uses the wvalue(list) constructor (list =
// std::vector<wvalue>) so an empty list serializes as [] (not null).
inline crow::json::wvalue injuryListToJson(const std::vector<kinetiq::domain::Injury>& injuries) {
    std::vector<crow::json::wvalue> items;
    items.reserve(injuries.size());
    for (const auto& injury : injuries) items.push_back(injuryToJson(injury));
    return crow::json::wvalue(items);
}

}  // namespace kinetiq::http::dto
```

- [ ] **Step 4: Update `InjuryController.h`**

Replace `Server/src/http/controllers/InjuryController.h` in full:

```cpp
#pragma once

#include "ports/IInjuryRepository.h"
#include "ports/IMuscleRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers CRUD routes for /api/v1/users/me/injuries on `app`. Free function
// (matches AuthController/UserController). Depends only on ports.
void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::IMuscleRepository& muscles,
                          kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
```

- [ ] **Step 5: Update `InjuryController.cpp`**

Replace `Server/src/http/controllers/InjuryController.cpp` in full:

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
                          kinetiq::ports::IMuscleRepository& muscles,
                          kinetiq::ports::ITokenService& tokens) {
    // Collection: GET (list) + POST (create).
    CROW_ROUTE(app, "/api/v1/users/me/injuries")
        .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)(
            [&injuries, &muscles, &tokens](const crow::request& req) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries, muscles);

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
            [&injuries, &muscles, &tokens](const crow::request& req,
                                           std::int64_t id) -> crow::response {
                const auto currentUser = authenticate(req, tokens);
                if (!currentUser.has_value()) {
                    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                                              "Missing, invalid, or expired token.");
                }
                kinetiq::app::InjuryService service(injuries, muscles);

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
                    return dto::errorResponse(404, "not_found", "Not Found", "Injury not found.");
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

```
cmake --build --preset default --target kinetiq_tests
./build/kinetiq_tests.exe --gtest_filter='InjuryControllerTest.*'
```

Expected: **PASS** — 12/12 (7 previous + 5 new: bad type, muscle+valid id, muscle+unknown id,
non-muscle+id, cascade-clear).

- [ ] **Step 7: Commit**

```
git add Server/src/http/dto/InjuryDto.h Server/src/http/controllers/InjuryController.h Server/src/http/controllers/InjuryController.cpp Server/tests/test_injury_controller.cpp
git commit -m "feat(server): extend InjuryDto/InjuryController for injury_type/muscle_id (TDD)"
```

---

### Task 8: Wire into `main.cpp` + full regression + live-verify

**Files:**
- Modify: `Server/src/main.cpp`

**Interfaces:**
- Consumes: `http::registerMuscleRoutes` (Task 6); grown `http::registerInjuryRoutes` signature (Task 7); a new `AutoCommitPgMuscleRepository` local (Task 4).

- [ ] **Step 1: Add the includes**

In `Server/src/main.cpp`, after `#include "adapters/db/AutoCommitPgInjuryRepository.h"`:

```cpp
#include "adapters/db/AutoCommitPgMuscleRepository.h"
```

After `#include "http/controllers/InjuryController.h"`:

```cpp
#include "http/controllers/MuscleController.h"
```

- [ ] **Step 2: Construct the repo + update route registration**

Replace the block from `kinetiq::db::AutoCommitPgUserRepository users(...)` through
`kinetiq::http::registerInjuryRoutes(...)`:

```cpp
    kinetiq::db::AutoCommitPgUserRepository users(buildConnString());
    kinetiq::db::AutoCommitPgInjuryRepository injuries(buildConnString());
    kinetiq::db::AutoCommitPgMuscleRepository muscles(buildConnString());
    kinetiq::crypto::Argon2PasswordHasher hasher;
    kinetiq::crypto::JwtTokenService tokens(jwtSecret, std::chrono::hours(24));
    kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
    kinetiq::http::registerUserRoutes(app, users, tokens);
    kinetiq::http::registerInjuryRoutes(app, injuries, muscles, tokens);
    kinetiq::http::registerMuscleRoutes(app, muscles, tokens);
```

- [ ] **Step 3: Build both targets**

```
cmake --build --preset default; echo exit=$?
```

Expected: `exit=0`.

- [ ] **Step 4: Full regression**

```
./build/kinetiq_tests.exe
```

Expected: all tests pass (previous suite + this feature's new tests). DB tests need Postgres up.

- [ ] **Step 5: Live-verify (Postgres up, server via `./scripts/run-dev.ps1 -Target server` or the exe)**

Register to get `$TOKEN`, then:

```
# list muscles (expect 200, 16 rows)
curl -s localhost:8080/api/v1/muscles -H "Authorization: Bearer $TOKEN"

# get a specific muscle by id (use an id from the list above, e.g. 1)
curl -s localhost:8080/api/v1/muscles/1 -H "Authorization: Bearer $TOKEN"

# unknown muscle id (expect 404)
curl -s -o /dev/null -w "%{http_code}\n" localhost:8080/api/v1/muscles/999999 -H "Authorization: Bearer $TOKEN"

# create a muscle-type injury with a valid muscle_id (expect 201; use the Hamstrings id from the list)
curl -s -X POST localhost:8080/api/v1/users/me/injuries -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"body_region":"hamstring","severity":"moderate","injury_type":"muscle","muscle_id":<HAMSTRINGS_ID>}'

# bad muscle_id (expect 422)
curl -s -o /dev/null -w "%{http_code}\n" -X POST localhost:8080/api/v1/users/me/injuries \
  -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"body_region":"hamstring","severity":"moderate","injury_type":"muscle","muscle_id":999999}'

# non-muscle type with muscle_id (expect 422)
curl -s -o /dev/null -w "%{http_code}\n" -X POST localhost:8080/api/v1/users/me/injuries \
  -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
  -d '{"body_region":"shin","severity":"mild","injury_type":"bone","muscle_id":<HAMSTRINGS_ID>}'

# PATCH the muscle injury's type away from "muscle" -- expect muscle_id cleared (null) in the response
curl -s -X PATCH localhost:8080/api/v1/users/me/injuries/<ID> -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" -d '{"injury_type":"bone"}'
```

Confirm each matches. Delete the test user from the dev DB (cascades injuries) and stop the server.

- [ ] **Step 6: Commit**

```
git add Server/src/main.cpp
git commit -m "feat(server): wire /muscles + updated injury routes into main; live-verified"
```

---

## Done — definition of done

- `injury_type` + `muscle_id` fully modeled (domain, repository, service, HTTP); `GET /muscles` +
  `GET /muscles/{id}` implemented; ownership + muscle-existence + cross-field validation all
  enforced; `ctest` fully green (DB tests pass with Postgres up); live-verified.
- **Next:** update `WORKLOG.md` / `CONTEXT.md` / the living `phase-2-slice-2-profile-injuries.md`
  build log with this extension's narrative, mirroring Stage A/B. Then continue to Stage C (`#25`
  frontend), which can now build its muscle picker against `GET /muscles`.

---

## Self-review notes (author check against the spec)

- **Spec coverage:** E1 (injury_type + optional muscle_id) ✓ Task 1/5; E2 (single muscle, no join
  table) ✓ (schema/domain use one `muscle_id` column throughout); E3 (seed 16 muscles from the
  documented list) ✓ Task 1 migration; E4 (muscle_id never required; existence checked in service,
  not domain) ✓ Task 1 (`InjuryPolicy` has no DB access) + Task 5 (`requireMuscleExistsIfSet`); E5
  (update cascade auto-clears muscle_id) ✓ Task 5 + tested at both service and HTTP layers (Task
  5/7); E6 (`GET /muscles` now, 🔒) ✓ Task 6.
- **Existing-code impact table from the spec, fully covered:** `test_injury_policy.cpp` (Task 1),
  `test_injury_repository.cpp`/`test_pg_injury_repository.cpp`/`test_auto_commit_pg_injury_repository.cpp`
  (Task 2), `test_injury_service.cpp` (Task 5), `test_injury_controller.cpp` (Task 7), `main.cpp`
  (Task 8).
- **Type consistency:** `AddInjuryRequest`/`UpdateInjuryFields` field names/types identical across
  Task 5 (definition) and Task 7 (`parseAddInjury`/`parseUpdateInjury`). `IMuscleRepository` method
  set identical across Task 3 (port + fake) and Task 4 (Pg + AutoCommit adapters). `InjuryService`
  constructor signature `(IInjuryRepository&, IMuscleRepository&)` consistent across Task 5
  (definition), Task 7 (`InjuryController.cpp` usage), Task 8 (`main.cpp`). `registerInjuryRoutes`
  signature `(app, injuries, muscles, tokens)` consistent across Task 7 (header + impl + its own
  test fixture) and Task 8 (`main.cpp`).
- **Migration ordering:** confirmed via `Migrator.cpp` that files are sorted lexically by filename
  and `0003_...` sorts after the existing `0001_...`/`0002_...` — no renumbering conflicts.
- **Crow API verified against the vendored header before use (same discipline as Stage B):** `.i()`
  (int64_t accessor), `.b()` (bool accessor), `.t() == crow::json::type::Null`, and the
  `wvalue(std::vector<wvalue>)` list constructor all confirmed present in `crow_all.h`.
- **Watch item:** Task 1 Step 8 explicitly calls out that `ensureTestDatabaseReady()` silently
  SKIPs (not FAILs) DB tests if the migration throws — the `psql` verification step is there
  specifically to catch a broken migration that ctest output alone would hide.
