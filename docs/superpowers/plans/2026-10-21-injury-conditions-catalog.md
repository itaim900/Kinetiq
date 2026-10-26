# Injury Conditions Catalog Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Injuries affect several muscles. Known conditions come from a seeded catalog that locks
region, type and muscles; unknown injuries are described by the user from fixed lists.

**Architecture:** New read-only `ICatalogRepository` port (body regions, muscles, conditions) with
an in-memory fake and the usual Pg + AutoCommit adapters. `Injury` gains `condition` + `muscles`;
`PgInjuryRepository` resolves muscles from the catalog (known) or `user_injury_muscles` (unknown);
`InjuryService` enforces the two request shapes; a new `CatalogController` exposes 3 GET endpoints.

**Tech Stack:** C++20, Crow (vendored), libpqxx, GoogleTest, CMake+Ninja (MSYS2 UCRT64), Postgres 16.

**Spec:** [`docs/superpowers/specs/2026-10-21-injury-conditions-catalog-design.md`](../specs/2026-10-21-injury-conditions-catalog-design.md)

## Global Constraints
- **Every commit green.** The pre-commit hook runs all server tests; never commit red, never `--no-verify`.
- **Never edit an applied migration** (`0001`–`0003`). This plan adds `0004` (catalog) and `0005`
  (injury link). *Deviation from spec:* the spec says one `0004`; splitting lets Task 2 (catalog)
  commit green before the injury tables change in Task 3.
- Body regions: exactly the 14 slugs in the spec. Muscles: 28 (16 existing + 12). Conditions: 17.
  Condition-muscle rows: **34**.
- Known condition ⇒ `body_region`, `injury_type`, `muscle_ids` are **locked** (422 on that field).
- Unknown injury muscles get role `"primary"`; duplicate `muscle_ids` are de-duplicated.
- Muscles are always ordered primary first, then by name.
- Build/test from `Server/` with `C:\msys64\ucrt64\bin` on `PATH`:
  `cmake --build --preset default` then `./build/kinetiq_tests.exe --gtest_filter='...'`.
  Check the real exit code (`; echo exit=$?`). Postgres must be up (`docker start kinetiq-postgres`);
  DB tests **skip** if a migration is broken — verify migrations with `psql` as instructed.
- Stop the dev server before a full build (Windows locks a running `kinetiq_server.exe`).
- New `.cpp` files must be added by hand to `Server/CMakeLists.txt`.

---

### Task 1: Catalog domain types, `ICatalogRepository` port, in-memory fake

**Files:**
- Create: `Server/src/domain/InjuryMuscle.h`, `Server/src/domain/BodyRegion.h`, `Server/src/domain/InjuryCondition.h`
- Create: `Server/src/ports/ICatalogRepository.h`
- Create: `Server/tests/support/InMemoryCatalogRepository.h`
- Create: `Server/tests/test_catalog_repository.cpp`
- Modify: `Server/CMakeLists.txt` (add test file to `kinetiq_tests`)

**Interfaces:**
- Produces:
  - `domain::InjuryMuscle { long long muscleId; std::string name; std::string role; }`
  - `domain::BodyRegion { std::string slug; std::string name; }`
  - `domain::InjuryCondition { long long id; std::string slug, name; std::optional<std::string> icd11Code; std::string bodyRegion, injuryType; std::vector<InjuryMuscle> muscles; }`
  - `ports::ICatalogRepository` with `listBodyRegions()`, `bodyRegionExists(slug)`, `listMuscles()`,
    `findMuscle(id)`, `listConditions()`, `findCondition(id)`.
  - `testsupport::InMemoryCatalogRepository` with constants `kHamstrings=1, kQuads=2, kCalves=3,
    kGlutes=4, kHamstringStrain=1`; regions `knee`, `lower_back`, `thigh`.

- [ ] **Step 1: Write the failing test** — create `Server/tests/test_catalog_repository.cpp`:

```cpp
// Injury conditions catalog: ICatalogRepository contract, exercised against the
// in-memory fake. PgCatalogRepository (Task 2) satisfies the same contract.

#include <gtest/gtest.h>

#include "support/InMemoryCatalogRepository.h"

using kinetiq::testsupport::InMemoryCatalogRepository;

TEST(InMemoryCatalogRepositoryTest, ListBodyRegions_ReturnsSeeded) {
    InMemoryCatalogRepository catalog;
    EXPECT_EQ(catalog.listBodyRegions().size(), 3u);
}

TEST(InMemoryCatalogRepositoryTest, BodyRegionExists) {
    InMemoryCatalogRepository catalog;
    EXPECT_TRUE(catalog.bodyRegionExists("knee"));
    EXPECT_FALSE(catalog.bodyRegionExists("left_knee"));
}

TEST(InMemoryCatalogRepositoryTest, FindMuscle) {
    InMemoryCatalogRepository catalog;
    const auto m = catalog.findMuscle(InMemoryCatalogRepository::kHamstrings);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->name, "Hamstrings");
    EXPECT_FALSE(catalog.findMuscle(999).has_value());
}

TEST(InMemoryCatalogRepositoryTest, FindCondition_ReturnsMusclesWithRoles) {
    InMemoryCatalogRepository catalog;
    const auto c = catalog.findCondition(InMemoryCatalogRepository::kHamstringStrain);
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->bodyRegion, "thigh");
    EXPECT_EQ(c->injuryType, "muscle");
    EXPECT_EQ(c->icd11Code, "NC76.30");
    ASSERT_EQ(c->muscles.size(), 2u);
    EXPECT_EQ(c->muscles[0].name, "Hamstrings");
    EXPECT_EQ(c->muscles[0].role, "primary");
    EXPECT_EQ(c->muscles[1].role, "secondary");
}

TEST(InMemoryCatalogRepositoryTest, FindCondition_Unknown_ReturnsNullopt) {
    InMemoryCatalogRepository catalog;
    EXPECT_FALSE(catalog.findCondition(999).has_value());
    EXPECT_EQ(catalog.listConditions().size(), 1u);
}
```

Add `tests/test_catalog_repository.cpp` to the `kinetiq_tests` source list in `Server/CMakeLists.txt`
(after `tests/test_injury_repository.cpp`).

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?`
Expected: FAIL — `support/InMemoryCatalogRepository.h: No such file or directory`.

- [ ] **Step 3: Create the domain types**

`Server/src/domain/InjuryMuscle.h`:
```cpp
#pragma once

#include <string>

namespace kinetiq::domain {

// A muscle as it relates to an injury. role: "primary" = the injured tissue
// itself, "secondary" = involved / to be protected.
struct InjuryMuscle {
    long long muscleId = 0;
    std::string name;
    std::string role;
};

}  // namespace kinetiq::domain
```

`Server/src/domain/BodyRegion.h`:
```cpp
#pragma once

#include <string>

namespace kinetiq::domain {

// One of the 14 fixed body regions (e.g. slug "lower_back", name "Lower back").
// Shared by injuries and exercise contraindications so the safety filter matches.
struct BodyRegion {
    std::string slug;
    std::string name;
};

}  // namespace kinetiq::domain
```

`Server/src/domain/InjuryCondition.h`:
```cpp
#pragma once

#include "domain/InjuryMuscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::domain {

// A known injury condition from the seeded catalog (e.g. "ACL tear"). It fixes
// the region, type and affected muscles of any injury that references it.
struct InjuryCondition {
    long long id = 0;
    std::string slug;
    std::string name;
    std::optional<std::string> icd11Code;  // WHO ICD-11; null when none found
    std::string bodyRegion;                // body_regions slug
    std::string injuryType;                // muscle|bone|tendon_ligament|joint|other
    std::vector<InjuryMuscle> muscles;     // primary first, then by name
};

}  // namespace kinetiq::domain
```

- [ ] **Step 4: Create the port** — `Server/src/ports/ICatalogRepository.h`:
```cpp
#pragma once

#include "domain/BodyRegion.h"
#include "domain/InjuryCondition.h"
#include "domain/Muscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::ports {

// Read-only reference data the app ships with: body regions, muscles and known
// injury conditions. Implementations: InMemoryCatalogRepository (tests/support/,
// fake) and PgCatalogRepository (adapters/db/, real).
class ICatalogRepository {
public:
    virtual ~ICatalogRepository() = default;

    virtual std::vector<domain::BodyRegion> listBodyRegions() = 0;
    virtual bool bodyRegionExists(const std::string& slug) = 0;
    virtual std::vector<domain::Muscle> listMuscles() = 0;
    virtual std::optional<domain::Muscle> findMuscle(long long id) = 0;
    virtual std::vector<domain::InjuryCondition> listConditions() = 0;
    virtual std::optional<domain::InjuryCondition> findCondition(long long id) = 0;
};

}  // namespace kinetiq::ports
```

- [ ] **Step 5: Create the fake** — `Server/tests/support/InMemoryCatalogRepository.h`:
```cpp
#pragma once

#include "ports/ICatalogRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::testsupport {

// Hand-written fake for ICatalogRepository, pre-seeded with a tiny catalog.
// Ids are fixed constants so tests can refer to them.
class InMemoryCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    static constexpr long long kHamstrings = 1;
    static constexpr long long kQuads = 2;
    static constexpr long long kCalves = 3;
    static constexpr long long kGlutes = 4;
    static constexpr long long kHamstringStrain = 1;

    InMemoryCatalogRepository() {
        regions_ = {{"knee", "Knee"}, {"lower_back", "Lower back"}, {"thigh", "Thigh"}};
        muscles_ = {{kHamstrings, "Hamstrings", "legs"},
                    {kQuads, "Quads", "legs"},
                    {kCalves, "Calves", "legs"},
                    {kGlutes, "Glutes", "legs"}};
        kinetiq::domain::InjuryCondition c;
        c.id = kHamstringStrain;
        c.slug = "hamstring_strain";
        c.name = "Hamstring strain";
        c.icd11Code = "NC76.30";
        c.bodyRegion = "thigh";
        c.injuryType = "muscle";
        c.muscles = {{kHamstrings, "Hamstrings", "primary"}, {kGlutes, "Glutes", "secondary"}};
        conditions_.push_back(c);
    }

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override { return regions_; }

    bool bodyRegionExists(const std::string& slug) override {
        for (const auto& r : regions_) {
            if (r.slug == slug) return true;
        }
        return false;
    }

    std::vector<kinetiq::domain::Muscle> listMuscles() override { return muscles_; }

    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override {
        for (const auto& m : muscles_) {
            if (m.id == id) return m;
        }
        return std::nullopt;
    }

    std::vector<kinetiq::domain::InjuryCondition> listConditions() override { return conditions_; }

    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override {
        for (const auto& c : conditions_) {
            if (c.id == id) return c;
        }
        return std::nullopt;
    }

private:
    std::vector<kinetiq::domain::BodyRegion> regions_;
    std::vector<kinetiq::domain::Muscle> muscles_;
    std::vector<kinetiq::domain::InjuryCondition> conditions_;
};

}  // namespace kinetiq::testsupport
```

- [ ] **Step 6: Run to verify it passes**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?` then
`./build/kinetiq_tests.exe --gtest_filter='InMemoryCatalogRepositoryTest.*'`
Expected: 5/5 PASS. Then full suite `./build/kinetiq_tests.exe | tail -3`: all pass, 0 skipped.

- [ ] **Step 7: Commit** (hook runs server checks)
```bash
git add Server/src/domain/InjuryMuscle.h Server/src/domain/BodyRegion.h Server/src/domain/InjuryCondition.h Server/src/ports/ICatalogRepository.h Server/tests/support/InMemoryCatalogRepository.h Server/tests/test_catalog_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): catalog domain types + ICatalogRepository port + fake (TDD)"
```

---

### Task 2: Migration `0004` + `PgCatalogRepository` + `AutoCommitPgCatalogRepository`

**Files:**
- Create: `Server/migrations/0004_injury_conditions_catalog.sql`
- Create: `Server/src/adapters/db/PgCatalogRepository.h` / `.cpp`
- Create: `Server/src/adapters/db/AutoCommitPgCatalogRepository.h` / `.cpp`
- Create: `Server/tests/test_pg_catalog_repository.cpp`, `Server/tests/test_auto_commit_pg_catalog_repository.cpp`
- Modify: `Server/tests/test_pg_injury_repository.cpp` (regions `left_knee`→`knee`, `hamstring`→`thigh`)
- Modify: `Server/CMakeLists.txt` (2 `.cpp` into `kinetiq_server` and `kinetiq_tests`; 2 test files)

**Interfaces:**
- Consumes: Task 1 types and port.
- Produces: `db::PgCatalogRepository(pqxx::work&)`, `db::AutoCommitPgCatalogRepository(std::string connString)`, both `: ports::ICatalogRepository`.

- [ ] **Step 1: Write the failing tests** — `Server/tests/test_pg_catalog_repository.cpp`:

```cpp
// Injury conditions catalog: migration 0004 content + PgCatalogRepository
// against real Postgres (DbTest per-test rollback).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgCatalogRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::PgCatalogRepository;

namespace {
long long count(pqxx::work& tx, const std::string& sql) {
    return tx.exec(sql)[0][0].as<long long>();
}
long long conditionId(pqxx::work& tx, const std::string& slug) {
    return tx.exec("SELECT id FROM injury_conditions WHERE slug = $1", pqxx::params{slug})[0][0]
        .as<long long>();
}
}  // namespace

TEST_F(DbTest, Migration0004_SeedsCatalog) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM body_regions"), 14);
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM muscles"), 28);
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM injury_conditions"), 17);
    // 34 = sum of every condition's muscles in the spec table. A typo in a
    // muscle name would silently drop a row (INSERT ... JOIN), so pin the total.
    EXPECT_EQ(count(tx(), "SELECT count(*) FROM injury_condition_muscles"), 34);
}

TEST_F(DbTest, Migration0004_EveryConditionHasAMuscle) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    EXPECT_EQ(count(tx(),
                    "SELECT count(*) FROM injury_conditions c WHERE NOT EXISTS "
                    "(SELECT 1 FROM injury_condition_muscles cm WHERE cm.condition_id = c.id)"),
              0);
}

TEST_F(DbTest, Migration0004_RejectsUnknownBodyRegion) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId =
        tx().exec("INSERT INTO users (email, password_hash) VALUES ('fk@example.com','x') RETURNING id")
            [0][0].as<long long>();
    EXPECT_THROW(tx().exec("INSERT INTO user_injuries (user_id, body_region, severity) "
                           "VALUES ($1, 'left_knee', 'mild')",
                           pqxx::params{userId}),
                 pqxx::sql_error);
}

TEST_F(DbTest, PgCatalogRepository_BodyRegions) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    EXPECT_EQ(catalog.listBodyRegions().size(), 14u);
    EXPECT_TRUE(catalog.bodyRegionExists("lower_back"));
    EXPECT_FALSE(catalog.bodyRegionExists("left_knee"));
}

TEST_F(DbTest, PgCatalogRepository_Muscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto all = catalog.listMuscles();
    EXPECT_EQ(all.size(), 28u);
    const auto found = catalog.findMuscle(all[0].id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, all[0].name);
    EXPECT_FALSE(catalog.findMuscle(999999).has_value());
}

TEST_F(DbTest, PgCatalogRepository_FindCondition_AclTear) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "acl_tear"));
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->name, "ACL tear");
    EXPECT_EQ(c->icd11Code, "NC93.62");
    EXPECT_EQ(c->bodyRegion, "knee");
    EXPECT_EQ(c->injuryType, "tendon_ligament");
    ASSERT_EQ(c->muscles.size(), 3u);
    for (const auto& m : c->muscles) EXPECT_EQ(m.role, "secondary");
}

TEST_F(DbTest, PgCatalogRepository_FindCondition_PrimaryFirst) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "hamstring_strain"));
    ASSERT_TRUE(c.has_value());
    ASSERT_EQ(c->muscles.size(), 2u);
    EXPECT_EQ(c->muscles[0].name, "Hamstrings");
    EXPECT_EQ(c->muscles[0].role, "primary");
    EXPECT_EQ(c->muscles[1].name, "Glutes");
    EXPECT_EQ(c->muscles[1].role, "secondary");
}

TEST_F(DbTest, PgCatalogRepository_NullIcdCode_AndListConditions) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    PgCatalogRepository catalog(tx());
    const auto c = catalog.findCondition(conditionId(tx(), "shin_splints"));
    ASSERT_TRUE(c.has_value());
    EXPECT_FALSE(c->icd11Code.has_value());
    const auto all = catalog.listConditions();
    EXPECT_EQ(all.size(), 17u);
    for (const auto& cond : all) EXPECT_FALSE(cond.muscles.empty()) << cond.slug;
    EXPECT_FALSE(catalog.findCondition(999999).has_value());
}
```

`Server/tests/test_auto_commit_pg_catalog_repository.cpp`:
```cpp
// AutoCommitPgCatalogRepository -- the catalog wired into main.cpp. Read-only,
// so nothing to clean up.

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgCatalogRepository.h"

using kinetiq::db::AutoCommitPgCatalogRepository;

TEST(AutoCommitPgCatalogRepositoryTest, ReadsCatalogOverOwnConnections) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }
    AutoCommitPgCatalogRepository catalog(kinetiq::testing::testConnString());
    EXPECT_EQ(catalog.listBodyRegions().size(), 14u);
    EXPECT_TRUE(catalog.bodyRegionExists("knee"));
    EXPECT_EQ(catalog.listMuscles().size(), 28u);
    const auto conditions = catalog.listConditions();
    ASSERT_EQ(conditions.size(), 17u);
    const auto again = catalog.findCondition(conditions[0].id);
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(again->muscles.size(), conditions[0].muscles.size());
    EXPECT_TRUE(catalog.findMuscle(catalog.listMuscles()[0].id).has_value());
}
```

In `Server/tests/test_pg_injury_repository.cpp` change `"left_knee"` → `"knee"` and
`make(userId, "hamstring", "moderate")` → `make(userId, "thigh", "moderate")` (the new FK rejects
free-text regions).

`Server/CMakeLists.txt`: add `src/adapters/db/PgCatalogRepository.cpp` and
`src/adapters/db/AutoCommitPgCatalogRepository.cpp` to **both** `kinetiq_server` and
`kinetiq_tests`; add `tests/test_pg_catalog_repository.cpp` and
`tests/test_auto_commit_pg_catalog_repository.cpp` to `kinetiq_tests`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?`
Expected: FAIL — `adapters/db/PgCatalogRepository.h: No such file or directory`.

- [ ] **Step 3: Write the migration** — `Server/migrations/0004_injury_conditions_catalog.sql`:

```sql
-- Migration 0004: injury conditions catalog (reference data).
--
-- Fixed body regions (shared by injuries and exercise contraindications so the
-- safety filter matches), 12 more muscles (28 total) and 17 known injury
-- conditions mapped to the muscles they affect. Medical sources and review
-- notes: docs/superpowers/specs/2026-10-21-injury-conditions-catalog-design.md.
-- Not medical advice; secondary muscles are project judgement pending physio review.

CREATE TABLE body_regions (
    slug TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

INSERT INTO body_regions (slug, name) VALUES
    ('neck', 'Neck'), ('shoulder', 'Shoulder'), ('upper_back', 'Upper back'),
    ('lower_back', 'Lower back'), ('chest', 'Chest'), ('abdomen', 'Abdomen'),
    ('elbow', 'Elbow'), ('wrist_hand', 'Wrist & hand'), ('hip', 'Hip'), ('groin', 'Groin'),
    ('thigh', 'Thigh'), ('knee', 'Knee'), ('lower_leg', 'Lower leg'),
    ('ankle_foot', 'Ankle & foot');

-- Normalise existing free-text regions, then remove rows that still don't
-- match a region (dev/test data only; no real users exist yet).
UPDATE user_injuries SET body_region = lower(replace(trim(body_region), ' ', '_'));
DELETE FROM user_injuries WHERE body_region NOT IN (SELECT slug FROM body_regions);
ALTER TABLE user_injuries
    ADD CONSTRAINT fk_user_injuries_body_region
    FOREIGN KEY (body_region) REFERENCES body_regions(slug);

DELETE FROM exercise_contraindications WHERE body_region NOT IN (SELECT slug FROM body_regions);
ALTER TABLE exercise_contraindications
    ADD CONSTRAINT fk_exercise_contraindications_body_region
    FOREIGN KEY (body_region) REFERENCES body_regions(slug);

INSERT INTO muscles (name, muscle_group) VALUES
    ('Supraspinatus', 'shoulders'), ('Infraspinatus', 'shoulders'),
    ('Teres Minor', 'shoulders'), ('Subscapularis', 'shoulders'),
    ('Forearm Flexors', 'arms'), ('Forearm Extensors', 'arms'),
    ('Tibialis Anterior', 'legs'), ('Adductors', 'legs'), ('Hip Flexors', 'legs'),
    ('Gluteus Medius', 'legs'), ('Levator Scapulae', 'neck'), ('Sternocleidomastoid', 'neck')
ON CONFLICT (name) DO NOTHING;

CREATE TABLE injury_conditions (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    slug        TEXT NOT NULL UNIQUE,
    name        TEXT NOT NULL UNIQUE,
    icd11_code  TEXT,                                   -- WHO ICD-11 MMS; NULL when none found
    body_region TEXT NOT NULL REFERENCES body_regions(slug),
    injury_type TEXT NOT NULL
        CHECK (injury_type IN ('muscle','bone','tendon_ligament','joint','other'))
);

CREATE TABLE injury_condition_muscles (
    condition_id BIGINT NOT NULL REFERENCES injury_conditions(id) ON DELETE CASCADE,
    muscle_id    BIGINT NOT NULL REFERENCES muscles(id),
    role         TEXT   NOT NULL CHECK (role IN ('primary','secondary')),
    PRIMARY KEY (condition_id, muscle_id)
);

INSERT INTO injury_conditions (slug, name, icd11_code, body_region, injury_type) VALUES
    ('acl_tear',              'ACL tear',              'NC93.62', 'knee',       'tendon_ligament'),
    ('meniscus_tear',         'Meniscus tear',         'NC93.3',  'knee',       'joint'),
    ('patellar_tendinopathy', 'Patellar tendinopathy', NULL,      'knee',       'tendon_ligament'),
    ('hamstring_strain',      'Hamstring strain',      'NC76.30', 'thigh',      'muscle'),
    ('quadriceps_strain',     'Quadriceps strain',     'NC76.10', 'thigh',      'muscle'),
    ('groin_strain',          'Groin strain',          'NC76.20', 'groin',      'muscle'),
    ('hip_flexor_strain',     'Hip flexor strain',     'NC76.00', 'hip',        'muscle'),
    ('calf_strain',           'Calf strain',           'NC96.10', 'lower_leg',  'muscle'),
    ('shin_splints',          'Shin splints',          NULL,      'lower_leg',  'bone'),
    ('achilles_tendinopathy', 'Achilles tendinopathy', NULL,      'ankle_foot', 'tendon_ligament'),
    ('ankle_sprain',          'Ankle sprain',          'ND14.7',  'ankle_foot', 'tendon_ligament'),
    ('lower_back_strain',     'Lower back strain',     'NB53.5',  'lower_back', 'muscle'),
    ('rotator_cuff_strain',   'Rotator cuff strain',   'NC16.00', 'shoulder',   'tendon_ligament'),
    ('tennis_elbow',          'Tennis elbow',          'FB55.1',  'elbow',      'tendon_ligament'),
    ('golfers_elbow',         'Golfer''s elbow',       'FB55.0',  'elbow',      'tendon_ligament'),
    ('neck_strain',           'Neck strain',           'NA23.4',  'neck',       'muscle'),
    ('pectoral_strain',       'Pectoral strain',       'NB34.0',  'chest',      'muscle');

-- Muscles are looked up by name: a typo silently drops a row, which the test
-- suite catches by pinning the total (34).
INSERT INTO injury_condition_muscles (condition_id, muscle_id, role)
SELECT c.id, m.id, v.role
FROM (VALUES
    ('acl_tear', 'Quads', 'secondary'), ('acl_tear', 'Hamstrings', 'secondary'),
    ('acl_tear', 'Calves', 'secondary'),
    ('meniscus_tear', 'Quads', 'secondary'), ('meniscus_tear', 'Hamstrings', 'secondary'),
    ('patellar_tendinopathy', 'Quads', 'secondary'),
    ('hamstring_strain', 'Hamstrings', 'primary'), ('hamstring_strain', 'Glutes', 'secondary'),
    ('quadriceps_strain', 'Quads', 'primary'), ('quadriceps_strain', 'Hip Flexors', 'secondary'),
    ('groin_strain', 'Adductors', 'primary'),
    ('hip_flexor_strain', 'Hip Flexors', 'primary'), ('hip_flexor_strain', 'Quads', 'secondary'),
    ('calf_strain', 'Calves', 'primary'),
    ('shin_splints', 'Tibialis Anterior', 'secondary'), ('shin_splints', 'Calves', 'secondary'),
    ('achilles_tendinopathy', 'Calves', 'secondary'),
    ('ankle_sprain', 'Calves', 'secondary'), ('ankle_sprain', 'Tibialis Anterior', 'secondary'),
    ('lower_back_strain', 'Erector Spinae', 'primary'),
    ('lower_back_strain', 'Glutes', 'secondary'), ('lower_back_strain', 'Obliques', 'secondary'),
    ('rotator_cuff_strain', 'Supraspinatus', 'primary'),
    ('rotator_cuff_strain', 'Infraspinatus', 'secondary'),
    ('rotator_cuff_strain', 'Teres Minor', 'secondary'),
    ('rotator_cuff_strain', 'Subscapularis', 'secondary'),
    ('rotator_cuff_strain', 'Lateral Deltoid', 'secondary'),
    ('tennis_elbow', 'Forearm Extensors', 'primary'),
    ('golfers_elbow', 'Forearm Flexors', 'primary'),
    ('neck_strain', 'Levator Scapulae', 'primary'),
    ('neck_strain', 'Sternocleidomastoid', 'primary'), ('neck_strain', 'Traps', 'secondary'),
    ('pectoral_strain', 'Pectorals', 'primary'), ('pectoral_strain', 'Anterior Deltoid', 'secondary')
) AS v(slug, muscle, role)
JOIN injury_conditions c ON c.slug = v.slug
JOIN muscles m ON m.name = v.muscle;
```

- [ ] **Step 4: Write `PgCatalogRepository`**

`Server/src/adapters/db/PgCatalogRepository.h`:
```cpp
#pragma once

#include "ports/ICatalogRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed ICatalogRepository over the reference tables from migration
// 0004. Operates within a transaction it is handed (DbTest rollback in tests,
// a per-op transaction in prod via AutoCommitPgCatalogRepository).
class PgCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    explicit PgCatalogRepository(pqxx::work& tx) : tx_(tx) {}

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override;
    bool bodyRegionExists(const std::string& slug) override;
    std::vector<kinetiq::domain::Muscle> listMuscles() override;
    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override;
    std::vector<kinetiq::domain::InjuryCondition> listConditions() override;
    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override;

private:
    // A condition's muscles, primary first, then by name.
    std::vector<kinetiq::domain::InjuryMuscle> conditionMuscles(long long conditionId);

    pqxx::work& tx_;
};

}  // namespace kinetiq::db
```

`Server/src/adapters/db/PgCatalogRepository.cpp`:
```cpp
#include "adapters/db/PgCatalogRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::Muscle rowToMuscle(const pqxx::row& row) {
    return {row["id"].as<long long>(), row["name"].as<std::string>(),
            row["muscle_group"].as<std::string>()};
}

constexpr const char* kConditionColumns =
    "SELECT id, slug, name, icd11_code, body_region, injury_type FROM injury_conditions";

}  // namespace

std::vector<kinetiq::domain::BodyRegion> PgCatalogRepository::listBodyRegions() {
    std::vector<kinetiq::domain::BodyRegion> out;
    for (const auto& row : tx_.exec("SELECT slug, name FROM body_regions ORDER BY name")) {
        out.push_back({row["slug"].as<std::string>(), row["name"].as<std::string>()});
    }
    return out;
}

bool PgCatalogRepository::bodyRegionExists(const std::string& slug) {
    return !tx_.exec("SELECT 1 FROM body_regions WHERE slug = $1", pqxx::params{slug}).empty();
}

std::vector<kinetiq::domain::Muscle> PgCatalogRepository::listMuscles() {
    std::vector<kinetiq::domain::Muscle> out;
    for (const auto& row :
         tx_.exec("SELECT id, name, muscle_group FROM muscles ORDER BY muscle_group, name")) {
        out.push_back(rowToMuscle(row));
    }
    return out;
}

std::optional<kinetiq::domain::Muscle> PgCatalogRepository::findMuscle(long long id) {
    const auto result =
        tx_.exec("SELECT id, name, muscle_group FROM muscles WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToMuscle(result[0]);
}

std::vector<kinetiq::domain::InjuryMuscle> PgCatalogRepository::conditionMuscles(
    long long conditionId) {
    std::vector<kinetiq::domain::InjuryMuscle> out;
    // (role = 'secondary') is false for primary rows, and false sorts first.
    for (const auto& row : tx_.exec(
             "SELECT m.id, m.name, cm.role FROM injury_condition_muscles cm "
             "JOIN muscles m ON m.id = cm.muscle_id WHERE cm.condition_id = $1 "
             "ORDER BY (cm.role = 'secondary'), m.name",
             pqxx::params{conditionId})) {
        out.push_back({row["id"].as<long long>(), row["name"].as<std::string>(),
                       row["role"].as<std::string>()});
    }
    return out;
}

namespace {
kinetiq::domain::InjuryCondition rowToCondition(const pqxx::row& row) {
    kinetiq::domain::InjuryCondition c;
    c.id = row["id"].as<long long>();
    c.slug = row["slug"].as<std::string>();
    c.name = row["name"].as<std::string>();
    if (!row["icd11_code"].is_null()) c.icd11Code = row["icd11_code"].as<std::string>();
    c.bodyRegion = row["body_region"].as<std::string>();
    c.injuryType = row["injury_type"].as<std::string>();
    return c;
}
}  // namespace

std::vector<kinetiq::domain::InjuryCondition> PgCatalogRepository::listConditions() {
    std::vector<kinetiq::domain::InjuryCondition> out;
    // One muscles query per condition: 17 small queries in one transaction is
    // fine for a reference list this size, and keeps the code simple.
    for (const auto& row : tx_.exec(std::string(kConditionColumns) + " ORDER BY name")) {
        auto c = rowToCondition(row);
        c.muscles = conditionMuscles(c.id);
        out.push_back(std::move(c));
    }
    return out;
}

std::optional<kinetiq::domain::InjuryCondition> PgCatalogRepository::findCondition(long long id) {
    const auto result =
        tx_.exec(std::string(kConditionColumns) + " WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    auto c = rowToCondition(result[0]);
    c.muscles = conditionMuscles(c.id);
    return c;
}

}  // namespace kinetiq::db
```

- [ ] **Step 5: Write `AutoCommitPgCatalogRepository`**

`Server/src/adapters/db/AutoCommitPgCatalogRepository.h`:
```cpp
#pragma once

#include "ports/ICatalogRepository.h"

#include <string>

namespace kinetiq::db {

// Production ICatalogRepository: a fresh connection + transaction per call,
// delegating to PgCatalogRepository. Same rationale as the other AutoCommit
// repositories (routes capture deps for the server's lifetime; a pqxx
// transaction must not be shared across concurrent requests).
class AutoCommitPgCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    explicit AutoCommitPgCatalogRepository(std::string connString)
        : connString_(std::move(connString)) {}

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override;
    bool bodyRegionExists(const std::string& slug) override;
    std::vector<kinetiq::domain::Muscle> listMuscles() override;
    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override;
    std::vector<kinetiq::domain::InjuryCondition> listConditions() override;
    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
```

`Server/src/adapters/db/AutoCommitPgCatalogRepository.cpp`:
```cpp
#include "adapters/db/AutoCommitPgCatalogRepository.h"

#include "adapters/db/PgCatalogRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

namespace {
// Runs `fn` against a PgCatalogRepository inside its own connection +
// transaction. Read-only, but commit anyway so the transaction ends cleanly.
template <typename Fn>
auto withCatalog(const std::string& connString, Fn fn) {
    pqxx::connection conn(connString);
    pqxx::work tx(conn);
    PgCatalogRepository repo(tx);
    auto result = fn(repo);
    tx.commit();
    return result;
}
}  // namespace

std::vector<kinetiq::domain::BodyRegion> AutoCommitPgCatalogRepository::listBodyRegions() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listBodyRegions(); });
}

bool AutoCommitPgCatalogRepository::bodyRegionExists(const std::string& slug) {
    return withCatalog(connString_,
                       [&slug](PgCatalogRepository& r) { return r.bodyRegionExists(slug); });
}

std::vector<kinetiq::domain::Muscle> AutoCommitPgCatalogRepository::listMuscles() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listMuscles(); });
}

std::optional<kinetiq::domain::Muscle> AutoCommitPgCatalogRepository::findMuscle(long long id) {
    return withCatalog(connString_, [id](PgCatalogRepository& r) { return r.findMuscle(id); });
}

std::vector<kinetiq::domain::InjuryCondition> AutoCommitPgCatalogRepository::listConditions() {
    return withCatalog(connString_, [](PgCatalogRepository& r) { return r.listConditions(); });
}

std::optional<kinetiq::domain::InjuryCondition> AutoCommitPgCatalogRepository::findCondition(
    long long id) {
    return withCatalog(connString_, [id](PgCatalogRepository& r) { return r.findCondition(id); });
}

}  // namespace kinetiq::db
```

- [ ] **Step 6: Build, run, and verify the migration directly**

Run: `cmake --build --preset default; echo exit=$?` then
`./build/kinetiq_tests.exe --gtest_filter='*Catalog*:*Migration0004*:*InjuryRepository*' 2>&1 | grep -E "testdb|^\[  (PASSED|FAILED|SKIPPED)"`
Expected: all PASS, **no SKIPPED, no `[testdb] migrating test DB failed` line**. Then:
```
docker exec kinetiq-postgres psql -U kinetiq -d kinetiq_test -c "SELECT version FROM schema_migrations ORDER BY 1" -c "SELECT c.name, count(*) FROM injury_conditions c JOIN injury_condition_muscles cm ON cm.condition_id = c.id GROUP BY c.name ORDER BY c.name"
```
Expected: `0004_injury_conditions_catalog` listed; 17 rows whose counts sum to 34. Full suite: all pass, 0 skipped.

- [ ] **Step 7: Commit**
```bash
git add Server/migrations/0004_injury_conditions_catalog.sql Server/src/adapters/db/PgCatalogRepository.h Server/src/adapters/db/PgCatalogRepository.cpp Server/src/adapters/db/AutoCommitPgCatalogRepository.h Server/src/adapters/db/AutoCommitPgCatalogRepository.cpp Server/tests/test_pg_catalog_repository.cpp Server/tests/test_auto_commit_pg_catalog_repository.cpp Server/tests/test_pg_injury_repository.cpp Server/CMakeLists.txt
git commit -m "feat(server): migration 0004 conditions catalog + Pg catalog repositories (TDD)"
```

---

### Task 3: Migration `0005` + multi-muscle `Injury` + `PgInjuryRepository`

**Files:**
- Create: `Server/migrations/0005_injury_condition_link.sql`
- Modify: `Server/src/domain/Injury.h`, `Server/src/domain/InjuryPolicy.h`, `Server/src/ports/IInjuryRepository.h` (comment)
- Modify: `Server/src/adapters/db/PgInjuryRepository.h` / `.cpp`
- Modify: `Server/tests/test_injury_policy.cpp`, `Server/tests/test_injury_repository.cpp`, `Server/tests/test_pg_injury_repository.cpp`

**Interfaces:**
- Consumes: `domain::InjuryMuscle` (Task 1); tables from `0004` (Task 2).
- Produces: `domain::ConditionRef { long long id; std::string name; std::optional<std::string> icd11Code; }`;
  `Injury` gains `std::optional<ConditionRef> condition` + `std::vector<InjuryMuscle> muscles`,
  **loses `muscleId`**. `PgInjuryRepository` persists `condition_id` and (unknown injuries) the muscle
  set; reads return `condition` + `muscles` resolved from the right source.

- [ ] **Step 1: Write the failing tests**

`Server/tests/test_injury_policy.cpp`: **delete** `MuscleIdWithNonMuscleType_Throws_FieldIsMuscleId`
and `MuscleIdWithMuscleType_DoesNotThrow` (the rule is gone), and append:
```cpp
// The July "muscles only for muscle-type injuries" rule is gone: a ligament
// injury (e.g. ACL) still involves muscles.
TEST(InjuryPolicy, LigamentInjuryWithMuscles_DoesNotThrow) {
    Injury i = validInjury();
    i.injuryType = "tendon_ligament";
    i.muscles = {{1, "Quads", "secondary"}};
    EXPECT_NO_THROW(requireValidInjury(i));
}
```

`Server/tests/test_injury_repository.cpp`: replace `Create_PersistsInjuryTypeAndMuscleId` with:
```cpp
// condition/muscles round-trip with zero fake-code changes (stores by value).
TEST(InMemoryInjuryRepositoryTest, Create_PersistsConditionAndMuscles) {
    InMemoryInjuryRepository repo;
    Injury i = make(1, "thigh", "moderate");
    i.injuryType = "muscle";
    i.condition = kinetiq::domain::ConditionRef{4, "Hamstring strain", "NC76.30"};
    i.muscles = {{15, "Hamstrings", "primary"}};
    const Injury saved = repo.create(i);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->condition.has_value());
    EXPECT_EQ(found->condition->name, "Hamstring strain");
    ASSERT_EQ(found->muscles.size(), 1u);
    EXPECT_EQ(found->muscles[0].name, "Hamstrings");
}
```

`Server/tests/test_pg_injury_repository.cpp`: replace the trailing `findMuscleIdByName` block and
`PgInjuryRepository_Create_PersistsInjuryTypeAndMuscleId` with:
```cpp
namespace {
long long idOf(pqxx::work& tx, const std::string& sql, const std::string& key) {
    return tx.exec(sql, pqxx::params{key})[0][0].as<long long>();
}
long long muscleId(pqxx::work& tx, const std::string& name) {
    return idOf(tx, "SELECT id FROM muscles WHERE name = $1", name);
}
long long conditionIdOf(pqxx::work& tx, const std::string& slug) {
    return idOf(tx, "SELECT id FROM injury_conditions WHERE slug = $1", slug);
}
}  // namespace

TEST_F(DbTest, PgInjuryRepository_UnknownInjury_PersistsUserMuscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "pg-injury-unknown@example.com");
    const long long hamstrings = muscleId(tx(), "Hamstrings");
    PgInjuryRepository repo(tx());

    Injury injury = make(userId, "thigh", "moderate");
    injury.injuryType = "muscle";
    injury.muscles = {{hamstrings, "Hamstrings", "primary"}};
    const Injury saved = repo.create(injury);

    EXPECT_FALSE(saved.condition.has_value());
    ASSERT_EQ(saved.muscles.size(), 1u);
    EXPECT_EQ(saved.muscles[0].muscleId, hamstrings);
    EXPECT_EQ(saved.muscles[0].name, "Hamstrings");
    EXPECT_EQ(saved.muscles[0].role, "primary");
}

TEST_F(DbTest, PgInjuryRepository_KnownCondition_ReadsCatalogMuscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "pg-injury-known@example.com");
    const long long acl = conditionIdOf(tx(), "acl_tear");
    PgInjuryRepository repo(tx());

    Injury injury = make(userId, "knee", "severe");
    injury.injuryType = "tendon_ligament";
    injury.condition = kinetiq::domain::ConditionRef{acl, "ACL tear", "NC93.62"};
    const Injury saved = repo.create(injury);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_TRUE(found->condition.has_value());
    EXPECT_EQ(found->condition->id, acl);
    EXPECT_EQ(found->condition->name, "ACL tear");
    EXPECT_EQ(found->condition->icd11Code, "NC93.62");
    ASSERT_EQ(found->muscles.size(), 3u);  // from the catalog, not stored per user
    for (const auto& m : found->muscles) EXPECT_EQ(m.role, "secondary");
}

TEST_F(DbTest, PgInjuryRepository_Update_ReplacesUserMuscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "pg-injury-replace@example.com");
    const long long hamstrings = muscleId(tx(), "Hamstrings");
    const long long quads = muscleId(tx(), "Quads");
    PgInjuryRepository repo(tx());

    Injury injury = make(userId, "thigh", "mild");
    injury.injuryType = "muscle";
    injury.muscles = {{hamstrings, "Hamstrings", "primary"}};
    Injury saved = repo.create(injury);

    saved.muscles = {{quads, "Quads", "primary"}};
    repo.update(saved);
    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    ASSERT_EQ(found->muscles.size(), 1u);
    EXPECT_EQ(found->muscles[0].name, "Quads");

    saved.muscles.clear();
    repo.update(saved);
    EXPECT_TRUE(repo.findById(saved.id)->muscles.empty());
}

TEST_F(DbTest, PgInjuryRepository_Delete_CascadesUserMuscles) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "pg-injury-cascade@example.com");
    PgInjuryRepository repo(tx());

    Injury injury = make(userId, "thigh", "mild");
    injury.injuryType = "muscle";
    injury.muscles = {{muscleId(tx(), "Quads"), "Quads", "primary"}};
    const Injury saved = repo.create(injury);
    repo.deleteById(saved.id);

    EXPECT_EQ(tx().exec("SELECT count(*) FROM user_injury_muscles WHERE injury_id = $1",
                        pqxx::params{saved.id})[0][0].as<long long>(),
              0);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?`
Expected: FAIL — `'struct kinetiq::domain::Injury' has no member named 'muscles'` / `'ConditionRef' is not a member`.

- [ ] **Step 3: Write migration `0005`** — `Server/migrations/0005_injury_condition_link.sql`:
```sql
-- Migration 0005: link injuries to the conditions catalog; several muscles per injury.
--
-- Replaces 0003's single muscle_id (decision E2, superseded 2026-10-21). Known
-- injuries reference a condition and read its muscles from the catalog;
-- unknown injuries keep the user's own picks in user_injury_muscles.

ALTER TABLE user_injuries DROP CONSTRAINT chk_muscle_id_requires_muscle_type;
DROP INDEX idx_user_injuries_muscle_id;
ALTER TABLE user_injuries DROP COLUMN muscle_id;

ALTER TABLE user_injuries ADD COLUMN condition_id BIGINT REFERENCES injury_conditions(id);
CREATE INDEX idx_user_injuries_condition_id ON user_injuries(condition_id);

-- Only for unknown injuries (condition_id IS NULL) -- enforced by InjuryService,
-- since a CHECK can't look at another table.
CREATE TABLE user_injury_muscles (
    injury_id BIGINT NOT NULL REFERENCES user_injuries(id) ON DELETE CASCADE,
    muscle_id BIGINT NOT NULL REFERENCES muscles(id),
    PRIMARY KEY (injury_id, muscle_id)
);
```

- [ ] **Step 4: Grow `Injury`** — replace `Server/src/domain/Injury.h`:
```cpp
#pragma once

#include "domain/InjuryMuscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::domain {

// The catalog condition an injury refers to, as shown alongside the injury.
struct ConditionRef {
    long long id = 0;
    std::string name;
    std::optional<std::string> icd11Code;
};

// A user's injury. Either a KNOWN condition from the catalog (condition set:
// region, type and muscles come from the catalog and are locked) or an
// UNKNOWN injury the user described (condition empty: muscles are their picks).
// body_region drives the Phase 3 safety filter. Inactive injuries are kept for
// history but excluded from the filter.
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;              // body_regions slug (e.g. "lower_back")
    std::string severity;                // mild|moderate|severe
    std::string injuryType;              // muscle|bone|tendon_ligament|joint|other
    std::optional<ConditionRef> condition;
    std::vector<InjuryMuscle> muscles;   // primary first, then by name
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;               // ISO timestamp, set by the DB (read-only)
};

}  // namespace kinetiq::domain
```

- [ ] **Step 5: Drop the muscle rule from `InjuryPolicy`** — in `Server/src/domain/InjuryPolicy.h`
delete the block starting `// Cross-field rule (E1/E4)` through its closing `}` (the
`muscleId` check). Severity and type checks stay unchanged.

- [ ] **Step 6: Update the port comment** — in `Server/src/ports/IInjuryRepository.h` replace the
`update` comment with:
```cpp
    // Overwrites the mutable fields (body_region, severity, injury_type,
    // active, notes) of an existing injury matched by injury.id and, for
    // unknown injuries (no condition), replaces its muscle set.
```
and above `create`:
```cpp
    // Assigns an id (and created_at) and persists, including condition_id and,
    // for unknown injuries, the muscle set. Returns the stored injury as read
    // back (condition name + muscle names/roles resolved).
```

- [ ] **Step 7: Rewrite `PgInjuryRepository`**

In `Server/src/adapters/db/PgInjuryRepository.h` add a private helper above `pqxx::work& tx_;`:
```cpp
    // Fills injury.muscles: the catalog's muscles for a known condition, else
    // the user's own picks (role "primary").
    void loadMuscles(kinetiq::domain::Injury& injury);
    // Rewrites user_injury_muscles for an unknown injury.
    void writeUserMuscles(const kinetiq::domain::Injury& injury);
```

Replace `Server/src/adapters/db/PgInjuryRepository.cpp`:
```cpp
#include "adapters/db/PgInjuryRepository.h"

namespace kinetiq::db {

namespace {

constexpr const char* kSelect =
    "SELECT ui.id, ui.user_id, ui.body_region, ui.severity, ui.injury_type, ui.active, "
    "ui.notes, ui.created_at, ui.condition_id, ic.name AS condition_name, "
    "ic.icd11_code AS condition_icd11 "
    "FROM user_injuries ui LEFT JOIN injury_conditions ic ON ic.id = ui.condition_id";

kinetiq::domain::Injury rowToInjury(const pqxx::row& row) {
    kinetiq::domain::Injury injury;
    injury.id = row["id"].as<long long>();
    injury.userId = row["user_id"].as<long long>();
    injury.bodyRegion = row["body_region"].as<std::string>();
    injury.severity = row["severity"].is_null() ? "" : row["severity"].as<std::string>();
    injury.injuryType = row["injury_type"].as<std::string>();
    injury.active = row["active"].as<bool>();
    if (!row["notes"].is_null()) injury.notes = row["notes"].as<std::string>();
    injury.createdAt = row["created_at"].as<std::string>();
    if (!row["condition_id"].is_null()) {
        kinetiq::domain::ConditionRef ref;
        ref.id = row["condition_id"].as<long long>();
        ref.name = row["condition_name"].as<std::string>();
        if (!row["condition_icd11"].is_null()) {
            ref.icd11Code = row["condition_icd11"].as<std::string>();
        }
        injury.condition = ref;
    }
    return injury;
}

std::optional<long long> conditionIdOf(const kinetiq::domain::Injury& injury) {
    if (injury.condition) return injury.condition->id;
    return std::nullopt;
}

}  // namespace

void PgInjuryRepository::loadMuscles(kinetiq::domain::Injury& injury) {
    injury.muscles.clear();
    // (role = 'secondary') is false for primary rows, and false sorts first.
    const auto result =
        injury.condition
            ? tx_.exec("SELECT m.id, m.name, cm.role FROM injury_condition_muscles cm "
                       "JOIN muscles m ON m.id = cm.muscle_id WHERE cm.condition_id = $1 "
                       "ORDER BY (cm.role = 'secondary'), m.name",
                       pqxx::params{injury.condition->id})
            : tx_.exec("SELECT m.id, m.name, 'primary' AS role FROM user_injury_muscles um "
                       "JOIN muscles m ON m.id = um.muscle_id WHERE um.injury_id = $1 "
                       "ORDER BY m.name",
                       pqxx::params{injury.id});
    for (const auto& row : result) {
        injury.muscles.push_back({row["id"].as<long long>(), row["name"].as<std::string>(),
                                  row["role"].as<std::string>()});
    }
}

void PgInjuryRepository::writeUserMuscles(const kinetiq::domain::Injury& injury) {
    tx_.exec("DELETE FROM user_injury_muscles WHERE injury_id = $1", pqxx::params{injury.id});
    if (injury.condition) return;  // known conditions read muscles from the catalog
    for (const auto& m : injury.muscles) {
        tx_.exec("INSERT INTO user_injury_muscles (injury_id, muscle_id) VALUES ($1, $2)",
                 pqxx::params{injury.id, m.muscleId});
    }
}

kinetiq::domain::Injury PgInjuryRepository::create(const kinetiq::domain::Injury& injury) {
    const auto result = tx_.exec(
        "INSERT INTO user_injuries (user_id, body_region, severity, injury_type, condition_id, "
        "active, notes) VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
        pqxx::params{injury.userId, injury.bodyRegion, injury.severity, injury.injuryType,
                     conditionIdOf(injury), injury.active, injury.notes});
    kinetiq::domain::Injury stored = injury;
    stored.id = result[0][0].as<long long>();
    writeUserMuscles(stored);
    return *findById(stored.id);  // read back: created_at, names, roles
}

std::vector<kinetiq::domain::Injury> PgInjuryRepository::listByUser(long long userId) {
    const auto result = tx_.exec(std::string(kSelect) + " WHERE ui.user_id = $1 ORDER BY ui.id",
                                 pqxx::params{userId});
    std::vector<kinetiq::domain::Injury> out;
    out.reserve(result.size());
    for (const auto& row : result) {
        auto injury = rowToInjury(row);
        loadMuscles(injury);
        out.push_back(std::move(injury));
    }
    return out;
}

std::optional<kinetiq::domain::Injury> PgInjuryRepository::findById(long long id) {
    const auto result = tx_.exec(std::string(kSelect) + " WHERE ui.id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    auto injury = rowToInjury(result[0]);
    loadMuscles(injury);
    return injury;
}

void PgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    tx_.exec(
        "UPDATE user_injuries SET body_region = $1, severity = $2, injury_type = $3, "
        "active = $4, notes = $5 WHERE id = $6",
        pqxx::params{injury.bodyRegion, injury.severity, injury.injuryType, injury.active,
                     injury.notes, injury.id});
    writeUserMuscles(injury);
}

void PgInjuryRepository::deleteById(long long id) {
    tx_.exec("DELETE FROM user_injuries WHERE id = $1", pqxx::params{id});
}

}  // namespace kinetiq::db
```

- [ ] **Step 8: Build, run, verify**

Run: `cmake --build --preset default; echo exit=$?` then
`./build/kinetiq_tests.exe 2>&1 | grep -E "testdb|^\[  (PASSED|FAILED|SKIPPED)  ?\] [0-9]"`
Expected: all pass, 0 skipped, no `[testdb]` failure line. Then:
`docker exec kinetiq-postgres psql -U kinetiq -d kinetiq_test -c "\d user_injuries" -c "\d user_injury_muscles"`
Expected: `condition_id` present, `muscle_id` gone; `user_injury_muscles` exists.

- [ ] **Step 9: Commit**
```bash
git add Server/migrations/0005_injury_condition_link.sql Server/src/domain/Injury.h Server/src/domain/InjuryPolicy.h Server/src/ports/IInjuryRepository.h Server/src/adapters/db/PgInjuryRepository.h Server/src/adapters/db/PgInjuryRepository.cpp Server/tests/test_injury_policy.cpp Server/tests/test_injury_repository.cpp Server/tests/test_pg_injury_repository.cpp
git commit -m "feat(server): migration 0005 + multi-muscle injuries in PgInjuryRepository (TDD)"
```

---

### Task 4: `InjuryService` — known vs unknown injuries

**Files:**
- Modify: `Server/src/app/InjuryService.h`
- Modify: `Server/tests/test_injury_service.cpp` (replace in full)
- Modify: `Server/src/http/controllers/InjuryController.h` / `.cpp` (signature: + catalog)
- Modify: `Server/src/main.cpp` (wire `AutoCommitPgCatalogRepository`)
- Modify: `Server/tests/test_injury_controller.cpp` (fixture gains a catalog fake)

**Interfaces:**
- Consumes: `ports::ICatalogRepository`, `InMemoryCatalogRepository` (Task 1); `Injury` (Task 3).
- Produces:
  - `AddInjuryRequest { long long userId; std::optional<long long> conditionId; std::optional<std::string> bodyRegion, injuryType; std::optional<std::vector<long long>> muscleIds; std::string severity; std::optional<std::string> notes; }`
  - `UpdateInjuryFields` gains `std::optional<std::vector<long long>> muscleIds`.
  - `InjuryService(IInjuryRepository&, ICatalogRepository&)`.
  - `http::registerInjuryRoutes(crow::SimpleApp&, IInjuryRepository&, ICatalogRepository&, ITokenService&)`.

- [ ] **Step 1: Write the failing tests** — replace `Server/tests/test_injury_service.cpp`:
```cpp
// InjuryService: CRUD with an ownership check, plus the two injury shapes --
// KNOWN (condition_id; region/type/muscles come from the catalog and are
// locked) and UNKNOWN (user supplies region/type/muscle_ids).

#include <gtest/gtest.h>

#include "app/InjuryService.h"
#include "domain/InjuryPolicy.h"
#include "support/InMemoryCatalogRepository.h"
#include "support/InMemoryInjuryRepository.h"

using kinetiq::app::AddInjuryRequest;
using kinetiq::app::InjuryNotFoundError;
using kinetiq::app::InjuryService;
using kinetiq::app::UpdateInjuryFields;
using kinetiq::domain::Injury;
using kinetiq::domain::InjuryValidationError;
using kinetiq::testsupport::InMemoryCatalogRepository;
using kinetiq::testsupport::InMemoryInjuryRepository;

namespace {

struct Fixture {
    InMemoryInjuryRepository repo;
    InMemoryCatalogRepository catalog;
    InjuryService svc{repo, catalog};
};

AddInjuryRequest unknownReq(long long userId, const std::string& region,
                            const std::string& severity) {
    AddInjuryRequest r;
    r.userId = userId;
    r.bodyRegion = region;
    r.injuryType = "other";
    r.severity = severity;
    return r;
}

AddInjuryRequest knownReq(long long userId, long long conditionId) {
    AddInjuryRequest r;
    r.userId = userId;
    r.conditionId = conditionId;
    r.severity = "moderate";
    return r;
}

// Asserts f() throws InjuryValidationError naming `field`.
template <typename F>
void expectFieldError(F&& f, const std::string& field) {
    try {
        f();
        FAIL() << "expected InjuryValidationError on " << field;
    } catch (const InjuryValidationError& e) {
        EXPECT_EQ(e.field(), field);
    }
}

}  // namespace

// --- unknown injuries ---

TEST(InjuryService, Add_ValidInjury_CreatesAndAppearsInList) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    EXPECT_GT(created.id, 0);
    EXPECT_FALSE(created.condition.has_value());
    const auto list = f.svc.list(1);
    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].bodyRegion, "lower_back");
}

TEST(InjuryService, Add_BadSeverity_Throws) {
    Fixture f;
    expectFieldError([&] { f.svc.add(unknownReq(1, "lower_back", "critical")); }, "severity");
}

TEST(InjuryService, Add_BadInjuryType_Throws) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.injuryType = "sprain";
    expectFieldError([&] { f.svc.add(req); }, "injury_type");
}

TEST(InjuryService, Add_Unknown_MissingBodyRegion_ThrowsOnBodyRegion) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.bodyRegion.reset();
    expectFieldError([&] { f.svc.add(req); }, "body_region");
}

TEST(InjuryService, Add_Unknown_MissingInjuryType_ThrowsOnInjuryType) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "moderate");
    req.injuryType.reset();
    expectFieldError([&] { f.svc.add(req); }, "injury_type");
}

TEST(InjuryService, Add_Unknown_UnknownRegion_ThrowsOnBodyRegion) {
    Fixture f;
    expectFieldError([&] { f.svc.add(unknownReq(1, "left_knee", "mild")); }, "body_region");
}

TEST(InjuryService, Add_Unknown_WithMuscles_ResolvesNamesAsPrimary) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    const Injury created = f.svc.add(req);
    ASSERT_EQ(created.muscles.size(), 1u);
    EXPECT_EQ(created.muscles[0].name, "Quads");
    EXPECT_EQ(created.muscles[0].role, "primary");
}

TEST(InjuryService, Add_Unknown_DuplicateMuscleIds_Deduplicated) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads,
                                           InMemoryCatalogRepository::kQuads,
                                           InMemoryCatalogRepository::kCalves};
    EXPECT_EQ(f.svc.add(req).muscles.size(), 2u);
}

TEST(InjuryService, Add_Unknown_UnknownMuscleId_ThrowsOnMuscleIds) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{999};
    expectFieldError([&] { f.svc.add(req); }, "muscle_ids");
}

// --- known conditions ---

TEST(InjuryService, Add_Known_CopiesRegionTypeAndMuscles) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    ASSERT_TRUE(created.condition.has_value());
    EXPECT_EQ(created.condition->name, "Hamstring strain");
    EXPECT_EQ(created.condition->icd11Code, "NC76.30");
    EXPECT_EQ(created.bodyRegion, "thigh");
    EXPECT_EQ(created.injuryType, "muscle");
    ASSERT_EQ(created.muscles.size(), 2u);
    EXPECT_EQ(created.muscles[0].role, "primary");
}

TEST(InjuryService, Add_Known_UnknownConditionId_ThrowsOnConditionId) {
    Fixture f;
    expectFieldError([&] { f.svc.add(knownReq(1, 999)); }, "condition_id");
}

TEST(InjuryService, Add_Known_WithLockedFields_ThrowsOnThatField) {
    Fixture f;
    auto withRegion = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withRegion.bodyRegion = "knee";
    expectFieldError([&] { f.svc.add(withRegion); }, "body_region");

    auto withType = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withType.injuryType = "bone";
    expectFieldError([&] { f.svc.add(withType); }, "injury_type");

    auto withMuscles = knownReq(1, InMemoryCatalogRepository::kHamstringStrain);
    withMuscles.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    expectFieldError([&] { f.svc.add(withMuscles); }, "muscle_ids");
}

// --- update ---

TEST(InjuryService, Update_OwnInjury_ChangesFields) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_FALSE(f.svc.update(1, created.id, fields).active);
}

TEST(InjuryService, Update_ChangesInjuryType) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "mild"));
    UpdateInjuryFields fields;
    fields.injuryType = "bone";
    EXPECT_EQ(f.svc.update(1, created.id, fields).injuryType, "bone");
}

TEST(InjuryService, Update_Unknown_ReplacesAndClearsMuscles) {
    Fixture f;
    auto req = unknownReq(1, "thigh", "mild");
    req.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kQuads};
    const Injury created = f.svc.add(req);

    UpdateInjuryFields replace;
    replace.muscleIds = std::vector<long long>{InMemoryCatalogRepository::kCalves};
    const Injury replaced = f.svc.update(1, created.id, replace);
    ASSERT_EQ(replaced.muscles.size(), 1u);
    EXPECT_EQ(replaced.muscles[0].name, "Calves");

    UpdateInjuryFields clear;
    clear.muscleIds = std::vector<long long>{};
    EXPECT_TRUE(f.svc.update(1, created.id, clear).muscles.empty());
}

TEST(InjuryService, Update_Unknown_UnknownRegion_ThrowsOnBodyRegion) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "mild"));
    UpdateInjuryFields fields;
    fields.bodyRegion = "left_knee";
    expectFieldError([&] { f.svc.update(1, created.id, fields); }, "body_region");
}

TEST(InjuryService, Update_Known_LockedField_ThrowsOnField) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    UpdateInjuryFields fields;
    fields.injuryType = "bone";
    expectFieldError([&] { f.svc.update(1, created.id, fields); }, "injury_type");
}

TEST(InjuryService, Update_Known_SeverityAllowed) {
    Fixture f;
    const Injury created = f.svc.add(knownReq(1, InMemoryCatalogRepository::kHamstringStrain));
    UpdateInjuryFields fields;
    fields.severity = "severe";
    const Injury updated = f.svc.update(1, created.id, fields);
    EXPECT_EQ(updated.severity, "severe");
    EXPECT_EQ(updated.muscles.size(), 2u);  // catalog muscles untouched
}

// --- ownership ---

TEST(InjuryService, Update_OtherUsersInjury_ThrowsNotFound) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    UpdateInjuryFields fields;
    fields.active = false;
    EXPECT_THROW(f.svc.update(2, created.id, fields), InjuryNotFoundError);
}

TEST(InjuryService, Remove_OwnInjury_Deletes) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    f.svc.remove(1, created.id);
    EXPECT_TRUE(f.svc.list(1).empty());
}

TEST(InjuryService, Remove_OtherUsersInjury_ThrowsNotFound) {
    Fixture f;
    const Injury created = f.svc.add(unknownReq(1, "lower_back", "moderate"));
    EXPECT_THROW(f.svc.remove(2, created.id), InjuryNotFoundError);
    EXPECT_EQ(f.svc.list(1).size(), 1u);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?`
Expected: FAIL — no matching constructor `InjuryService(…, InMemoryCatalogRepository&)` /
`AddInjuryRequest` has no member `conditionId`.

- [ ] **Step 3: Rewrite `InjuryService`** — replace `Server/src/app/InjuryService.h`:
```cpp
#pragma once

#include "domain/Injury.h"
#include "domain/InjuryPolicy.h"
#include "ports/ICatalogRepository.h"
#include "ports/IInjuryRepository.h"

#include <algorithm>
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

// Two shapes: KNOWN (conditionId set; bodyRegion/injuryType/muscleIds must be
// absent) or UNKNOWN (bodyRegion + injuryType required, muscleIds optional).
struct AddInjuryRequest {
    long long userId = 0;
    std::optional<long long> conditionId;
    std::optional<std::string> bodyRegion;
    std::optional<std::string> injuryType;
    std::optional<std::vector<long long>> muscleIds;
    std::string severity;
    std::optional<std::string> notes;
};

// Partial update: present fields change, absent fields are left unchanged.
// For a KNOWN injury, bodyRegion/injuryType/muscleIds are locked.
// muscleIds replaces the whole set (an empty list clears it).
struct UpdateInjuryFields {
    std::optional<std::string> bodyRegion;
    std::optional<std::string> severity;
    std::optional<std::string> injuryType;
    std::optional<std::vector<long long>> muscleIds;
    std::optional<std::string> notes;
    std::optional<bool> active;
};

// Cohesive CRUD service for a user's injuries (one class because the
// operations share the ownership check). Uses the catalog to resolve known
// conditions and to check that regions and muscles exist.
class InjuryService {
public:
    InjuryService(kinetiq::ports::IInjuryRepository& injuries,
                  kinetiq::ports::ICatalogRepository& catalog)
        : injuries_(injuries), catalog_(catalog) {}

    kinetiq::domain::Injury add(const AddInjuryRequest& req) {
        kinetiq::domain::Injury injury;
        injury.userId = req.userId;
        injury.severity = req.severity;
        injury.notes = req.notes;
        injury.active = true;

        if (req.conditionId) {
            rejectLocked(req.bodyRegion.has_value(), "body_region");
            rejectLocked(req.injuryType.has_value(), "injury_type");
            rejectLocked(req.muscleIds.has_value(), "muscle_ids");
            const auto condition = catalog_.findCondition(*req.conditionId);
            if (!condition) {
                throw kinetiq::domain::InjuryValidationError(
                    "condition_id", "condition_id does not match a known condition");
            }
            injury.condition =
                kinetiq::domain::ConditionRef{condition->id, condition->name, condition->icd11Code};
            injury.bodyRegion = condition->bodyRegion;
            injury.injuryType = condition->injuryType;
            injury.muscles = condition->muscles;
        } else {
            if (!req.bodyRegion) {
                throw kinetiq::domain::InjuryValidationError(
                    "body_region", "body_region is required unless condition_id is given");
            }
            if (!req.injuryType) {
                throw kinetiq::domain::InjuryValidationError(
                    "injury_type", "injury_type is required unless condition_id is given");
            }
            injury.bodyRegion = *req.bodyRegion;
            injury.injuryType = *req.injuryType;
            injury.muscles = resolveMuscles(req.muscleIds.value_or(std::vector<long long>{}));
        }

        kinetiq::domain::requireValidInjury(injury);
        if (!injury.condition) requireKnownRegion(injury.bodyRegion);
        return injuries_.create(injury);
    }

    std::vector<kinetiq::domain::Injury> list(long long userId) {
        return injuries_.listByUser(userId);
    }

    kinetiq::domain::Injury update(long long userId, long long injuryId,
                                   const UpdateInjuryFields& fields) {
        kinetiq::domain::Injury injury = requireOwned(userId, injuryId);
        if (injury.condition) {
            rejectLocked(fields.bodyRegion.has_value(), "body_region");
            rejectLocked(fields.injuryType.has_value(), "injury_type");
            rejectLocked(fields.muscleIds.has_value(), "muscle_ids");
        } else {
            if (fields.bodyRegion) injury.bodyRegion = *fields.bodyRegion;
            if (fields.injuryType) injury.injuryType = *fields.injuryType;
            if (fields.muscleIds) injury.muscles = resolveMuscles(*fields.muscleIds);
        }
        if (fields.severity) injury.severity = *fields.severity;
        if (fields.notes) injury.notes = fields.notes;
        if (fields.active) injury.active = *fields.active;

        kinetiq::domain::requireValidInjury(injury);
        if (!injury.condition) requireKnownRegion(injury.bodyRegion);
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

    // A known condition fixes this field; sending it is an error, not ignored.
    static void rejectLocked(bool present, const std::string& field) {
        if (present) {
            throw kinetiq::domain::InjuryValidationError(
                field, field + " cannot be set for a known condition (it comes from the catalog)");
        }
    }

    void requireKnownRegion(const std::string& slug) {
        if (!catalog_.bodyRegionExists(slug)) {
            throw kinetiq::domain::InjuryValidationError("body_region",
                                                         "body_region is not a known region");
        }
    }

    // Looks up each id (de-duplicated, order kept) and returns the muscles with
    // role "primary". Throws on the first id the catalog doesn't know.
    std::vector<kinetiq::domain::InjuryMuscle> resolveMuscles(const std::vector<long long>& ids) {
        std::vector<kinetiq::domain::InjuryMuscle> out;
        for (long long id : ids) {
            const bool seen = std::any_of(out.begin(), out.end(),
                                          [id](const auto& m) { return m.muscleId == id; });
            if (seen) continue;
            const auto muscle = catalog_.findMuscle(id);
            if (!muscle) {
                throw kinetiq::domain::InjuryValidationError(
                    "muscle_ids", "muscle_ids contains an unknown muscle id: " + std::to_string(id));
            }
            out.push_back({muscle->id, muscle->name, "primary"});
        }
        return out;
    }

    kinetiq::ports::IInjuryRepository& injuries_;
    kinetiq::ports::ICatalogRepository& catalog_;
};

}  // namespace kinetiq::app
```

- [ ] **Step 4: Thread the catalog through the controller and `main.cpp`**

`Server/src/http/controllers/InjuryController.h`: add `#include "ports/ICatalogRepository.h"` and
change the declaration to
```cpp
void registerInjuryRoutes(crow::SimpleApp& app, kinetiq::ports::IInjuryRepository& injuries,
                          kinetiq::ports::ICatalogRepository& catalog,
                          kinetiq::ports::ITokenService& tokens);
```
`Server/src/http/controllers/InjuryController.cpp`: same signature change; in **both** route
lambdas change the capture list `[&injuries, &tokens]` → `[&injuries, &catalog, &tokens]` and
`kinetiq::app::InjuryService service(injuries);` → `kinetiq::app::InjuryService service(injuries, catalog);`.

`Server/src/main.cpp`: add `#include "adapters/db/AutoCommitPgCatalogRepository.h"`; after the
`injuries` line add `kinetiq::db::AutoCommitPgCatalogRepository catalog(buildConnString());` and
change the call to `kinetiq::http::registerInjuryRoutes(app, injuries, catalog, tokens);`.

`Server/tests/test_injury_controller.cpp`: add `#include "support/InMemoryCatalogRepository.h"`,
`using kinetiq::testsupport::InMemoryCatalogRepository;`, a member
`InMemoryCatalogRepository catalog;` in `InjuryApi` (after `injuries`), and change the registration
to `kinetiq::http::registerInjuryRoutes(app, injuries, catalog, tokens);`.

(The DTO still compiles: assigning a `std::string` to the now-optional `bodyRegion`/`injuryType`
works. The new request/response shapes come in Task 5.)

- [ ] **Step 5: Build, run, verify**

Run: `cmake --build --preset default; echo exit=$?` then `./build/kinetiq_tests.exe 2>&1 | grep -E "^\[  (PASSED|FAILED|SKIPPED)  ?\] [0-9]"`
Expected: all pass, 0 skipped.

- [ ] **Step 6: Commit**
```bash
git add Server/src/app/InjuryService.h Server/tests/test_injury_service.cpp Server/src/http/controllers/InjuryController.h Server/src/http/controllers/InjuryController.cpp Server/src/main.cpp Server/tests/test_injury_controller.cpp
git commit -m "feat(server): InjuryService known vs unknown injuries via catalog (TDD)"
```

---

### Task 5: HTTP — catalog endpoints + new injury request/response shapes

**Files:**
- Create: `Server/src/http/dto/CatalogDto.h`
- Create: `Server/src/http/controllers/CatalogController.h` / `.cpp`
- Create: `Server/tests/test_catalog_controller.cpp`
- Modify: `Server/src/http/dto/InjuryDto.h`
- Modify: `Server/tests/test_injury_controller.cpp`
- Modify: `Server/src/main.cpp`, `Server/CMakeLists.txt`

**Interfaces:**
- Consumes: `ICatalogRepository`, `InjuryService` request types (Task 4).
- Produces: `http::registerCatalogRoutes(crow::SimpleApp&, ICatalogRepository&, ITokenService&)`;
  `dto::injuryMuscleListToJson(const std::vector<domain::InjuryMuscle>&)` (shared by both DTOs).

- [ ] **Step 1: Write the failing tests**

`Server/tests/test_catalog_controller.cpp`:
```cpp
// Catalog endpoints: GET /body-regions, /muscles, /injury-conditions (all 🔒).

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/CatalogController.h"
#include "support/InMemoryCatalogRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryCatalogRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct CatalogApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryCatalogRepository catalog;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    CatalogApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerCatalogRoutes(app, catalog, tokens);
    }
};

template <typename App>
std::string tokenFor(kinetiq::testing::HttpClient<App>& client) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":"cat@example.com","password":"StrongPass1","display_name":"C"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(CatalogControllerTest, GetBodyRegions_Returns200AndList) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.get("/api/v1/body-regions",
                              {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(std::string(body[0]["slug"].s()), "knee");
    EXPECT_EQ(std::string(body[0]["name"].s()), "Knee");
}

TEST(CatalogControllerTest, GetMuscles_Returns200AndList) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r =
        client.get("/api/v1/muscles", {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_EQ(body.size(), 4u);
    EXPECT_EQ(std::string(body[0]["name"].s()), "Hamstrings");
    EXPECT_EQ(std::string(body[0]["muscle_group"].s()), "legs");
}

TEST(CatalogControllerTest, GetInjuryConditions_IncludesMusclesAndRoles) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.get("/api/v1/injury-conditions",
                              {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(std::string(body[0]["name"].s()), "Hamstring strain");
    EXPECT_EQ(std::string(body[0]["icd11_code"].s()), "NC76.30");
    EXPECT_EQ(std::string(body[0]["body_region"].s()), "thigh");
    ASSERT_EQ(body[0]["muscles"].size(), 2u);
    EXPECT_EQ(std::string(body[0]["muscles"][0]["role"].s()), "primary");
}

TEST(CatalogControllerTest, Unauthenticated_Returns401) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    for (const char* url : {"/api/v1/body-regions", "/api/v1/muscles", "/api/v1/injury-conditions"}) {
        EXPECT_EQ(client.get(url).code, 401) << url;
    }
}
```

Append to `Server/tests/test_injury_controller.cpp`:
```cpp
TEST(InjuryControllerTest, Create_KnownCondition_Returns201WithCatalogData) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-known@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"condition_id":1,"severity":"moderate"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 201);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["condition"]["name"].s()), "Hamstring strain");
    EXPECT_EQ(std::string(body["condition"]["icd11_code"].s()), "NC76.30");
    EXPECT_EQ(std::string(body["body_region"].s()), "thigh");
    ASSERT_EQ(body["muscles"].size(), 2u);
    EXPECT_EQ(std::string(body["muscles"][0]["name"].s()), "Hamstrings");
    EXPECT_EQ(std::string(body["muscles"][0]["role"].s()), "primary");
}

TEST(InjuryControllerTest, Create_KnownConditionWithLockedField_Returns422NamingField) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-locked@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"condition_id":1,"severity":"mild","muscle_ids":[2]})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_TRUE(body["errors"].has("muscle_ids"));
}

TEST(InjuryControllerTest, Create_UnknownWithMuscleIds_Returns201Deduplicated) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-muscles@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[2,2,3]})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 201);
    const auto body = crow::json::load(r.body);
    EXPECT_EQ(body["condition"].t(), crow::json::type::Null);
    EXPECT_EQ(body["muscles"].size(), 2u);
}

TEST(InjuryControllerTest, Create_UnknownMuscleId_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-badmuscle@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[999]})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("muscle_ids"));
}

TEST(InjuryControllerTest, Patch_KnownCondition_InjuryType_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-patch-known@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries", R"({"condition_id":1,"severity":"mild"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"injury_type":"bone"})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("injury_type"));
}

TEST(InjuryControllerTest, Patch_Unknown_MuscleIdsEmpty_ClearsMuscles) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-clear@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[2]})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"muscle_ids":[]})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(crow::json::load(r.body)["muscles"].size(), 0u);
}
```

`Server/CMakeLists.txt`: add `src/http/controllers/CatalogController.cpp` to **both** targets and
`tests/test_catalog_controller.cpp` to `kinetiq_tests`.

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build --preset default --target kinetiq_tests; echo exit=$?`
Expected: FAIL — `http/controllers/CatalogController.h: No such file or directory`.

- [ ] **Step 3: Write `CatalogDto.h`** — `Server/src/http/dto/CatalogDto.h`:
```cpp
#pragma once

#include "domain/BodyRegion.h"
#include "domain/InjuryCondition.h"
#include "domain/InjuryMuscle.h"
#include "domain/Muscle.h"

#include "crow_all.h"

#include <vector>

namespace kinetiq::http::dto {

// [{id, name, role}] -- shared by injury and condition responses. Built with
// the wvalue(vector) constructor so an empty list serializes as [] (not null).
inline crow::json::wvalue injuryMuscleListToJson(
    const std::vector<kinetiq::domain::InjuryMuscle>& muscles) {
    std::vector<crow::json::wvalue> items;
    for (const auto& m : muscles) {
        crow::json::wvalue j;
        j["id"] = m.muscleId;
        j["name"] = m.name;
        j["role"] = m.role;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue bodyRegionListToJson(
    const std::vector<kinetiq::domain::BodyRegion>& regions) {
    std::vector<crow::json::wvalue> items;
    for (const auto& r : regions) {
        crow::json::wvalue j;
        j["slug"] = r.slug;
        j["name"] = r.name;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue muscleListToJson(const std::vector<kinetiq::domain::Muscle>& muscles) {
    std::vector<crow::json::wvalue> items;
    for (const auto& m : muscles) {
        crow::json::wvalue j;
        j["id"] = m.id;
        j["name"] = m.name;
        j["muscle_group"] = m.muscleGroup;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue conditionListToJson(
    const std::vector<kinetiq::domain::InjuryCondition>& conditions) {
    std::vector<crow::json::wvalue> items;
    for (const auto& c : conditions) {
        crow::json::wvalue j;
        j["id"] = c.id;
        j["slug"] = c.slug;
        j["name"] = c.name;
        j["icd11_code"] =
            c.icd11Code ? crow::json::wvalue(*c.icd11Code) : crow::json::wvalue(nullptr);
        j["body_region"] = c.bodyRegion;
        j["injury_type"] = c.injuryType;
        j["muscles"] = injuryMuscleListToJson(c.muscles);
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

}  // namespace kinetiq::http::dto
```

- [ ] **Step 4: Write `CatalogController`**

`Server/src/http/controllers/CatalogController.h`:
```cpp
#pragma once

#include "ports/ICatalogRepository.h"
#include "ports/ITokenService.h"

#include "crow_all.h"

namespace kinetiq::http {

// Registers the read-only reference-data routes: GET /api/v1/body-regions,
// /api/v1/muscles, /api/v1/injury-conditions. All require a valid token.
void registerCatalogRoutes(crow::SimpleApp& app, kinetiq::ports::ICatalogRepository& catalog,
                           kinetiq::ports::ITokenService& tokens);

}  // namespace kinetiq::http
```

`Server/src/http/controllers/CatalogController.cpp`:
```cpp
#include "http/controllers/CatalogController.h"

#include "http/dto/AuthDto.h"  // dto::errorResponse
#include "http/dto/CatalogDto.h"
#include "http/middleware/Authenticator.h"

namespace kinetiq::http {

namespace {
crow::response unauthenticated() {
    return dto::errorResponse(401, "unauthenticated", "Unauthorized",
                              "Missing, invalid, or expired token.");
}
}  // namespace

void registerCatalogRoutes(crow::SimpleApp& app, kinetiq::ports::ICatalogRepository& catalog,
                           kinetiq::ports::ITokenService& tokens) {
    CROW_ROUTE(app, "/api/v1/body-regions")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::bodyRegionListToJson(catalog.listBodyRegions()));
    });

    CROW_ROUTE(app, "/api/v1/muscles")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::muscleListToJson(catalog.listMuscles()));
    });

    CROW_ROUTE(app, "/api/v1/injury-conditions")
    ([&catalog, &tokens](const crow::request& req) -> crow::response {
        if (!authenticate(req, tokens)) return unauthenticated();
        return crow::response(200, dto::conditionListToJson(catalog.listConditions()));
    });
}

}  // namespace kinetiq::http
```

- [ ] **Step 5: Update `InjuryDto.h`** — add `#include "http/dto/CatalogDto.h"` and replace
`parseAddInjury`, `parseUpdateInjury` and `injuryToJson`:
```cpp
// Reads a JSON array of integers (e.g. "muscle_ids": [2, 3]). A non-array
// throws, which the controller maps to a generic 422.
inline std::vector<long long> parseIdList(const crow::json::rvalue& value) {
    if (value.t() != crow::json::type::List) {
        throw std::runtime_error("expected a list of ids");
    }
    std::vector<long long> ids;
    for (const auto& item : value) ids.push_back(item.i());
    return ids;
}

// Parses POST body into an AddInjuryRequest, pairing it with the authenticated
// user id (never from the body). Only present keys are set; InjuryService
// decides whether the combination is a valid known/unknown shape.
inline kinetiq::app::AddInjuryRequest parseAddInjury(const crow::json::rvalue& body,
                                                     long long userId) {
    kinetiq::app::AddInjuryRequest req;
    req.userId = userId;
    if (body.has("condition_id")) req.conditionId = body["condition_id"].i();
    if (body.has("body_region")) req.bodyRegion = std::string(body["body_region"].s());
    if (body.has("injury_type")) req.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_ids")) req.muscleIds = parseIdList(body["muscle_ids"]);
    req.severity = std::string(body["severity"].s());
    if (body.has("notes")) req.notes = std::string(body["notes"].s());
    return req;
}

// Parses PATCH body into UpdateInjuryFields -- only present keys are set.
inline kinetiq::app::UpdateInjuryFields parseUpdateInjury(const crow::json::rvalue& body) {
    kinetiq::app::UpdateInjuryFields fields;
    if (body.has("body_region")) fields.bodyRegion = std::string(body["body_region"].s());
    if (body.has("severity")) fields.severity = std::string(body["severity"].s());
    if (body.has("injury_type")) fields.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_ids")) fields.muscleIds = parseIdList(body["muscle_ids"]);
    if (body.has("notes")) fields.notes = std::string(body["notes"].s());
    if (body.has("active")) fields.active = body["active"].b();
    return fields;
}

inline crow::json::wvalue injuryToJson(const kinetiq::domain::Injury& injury) {
    crow::json::wvalue json;
    json["id"] = injury.id;
    json["user_id"] = injury.userId;
    if (injury.condition) {
        crow::json::wvalue c;
        c["id"] = injury.condition->id;
        c["name"] = injury.condition->name;
        c["icd11_code"] = injury.condition->icd11Code
                              ? crow::json::wvalue(*injury.condition->icd11Code)
                              : crow::json::wvalue(nullptr);
        json["condition"] = std::move(c);
    } else {
        json["condition"] = crow::json::wvalue(nullptr);
    }
    json["body_region"] = injury.bodyRegion;
    json["injury_type"] = injury.injuryType;
    json["severity"] = injury.severity;
    json["muscles"] = injuryMuscleListToJson(injury.muscles);
    json["active"] = injury.active;
    json["notes"] = injury.notes ? crow::json::wvalue(*injury.notes) : crow::json::wvalue(nullptr);
    json["created_at"] = injury.createdAt;
    return json;
}
```
Also add `#include <stdexcept>` to `InjuryDto.h`.

- [ ] **Step 6: Wire the catalog routes in `main.cpp`** — add
`#include "http/controllers/CatalogController.h"` and, after `registerInjuryRoutes(...)`:
`kinetiq::http::registerCatalogRoutes(app, catalog, tokens);`

- [ ] **Step 7: Build, run, verify**

Run: `cmake --build --preset default; echo exit=$?` then `./build/kinetiq_tests.exe 2>&1 | grep -E "^\[  (PASSED|FAILED|SKIPPED)  ?\] [0-9]"`
Expected: all pass, 0 skipped.

- [ ] **Step 8: Commit**
```bash
git add Server/src/http/dto/CatalogDto.h Server/src/http/controllers/CatalogController.h Server/src/http/controllers/CatalogController.cpp Server/tests/test_catalog_controller.cpp Server/src/http/dto/InjuryDto.h Server/tests/test_injury_controller.cpp Server/src/main.cpp Server/CMakeLists.txt
git commit -m "feat(server): catalog endpoints + known/unknown injury shapes over HTTP (TDD)"
```

---

### Task 6: Live verification + documentation

**Files:**
- Modify: `docs/02-api-contract.md` (Injuries section + new Catalog section)
- Modify: `docs/01-data-model.md` (`User_Injuries`, `Muscles` sections)
- Modify: `docs/phase-2-slice-2-profile-injuries.md` (build log), `docs/CONTEXT.md`, `WORKLOG.md`

- [ ] **Step 1: Live-verify against the real server**

Start: `./scripts/run-dev.ps1 -Target server` (applies 0004 + 0005 to the dev DB). Then:
```bash
B=http://127.0.0.1:8080/api/v1; J="Content-Type: application/json"
TOKEN=$(curl -s -X POST $B/auth/register -H "$J" -d '{"email":"live-cat@example.com","password":"StrongPass1","display_name":"L"}' | sed -n 's/.*"access_token":"\([^"]*\)".*/\1/p')
H="Authorization: Bearer $TOKEN"
curl -s $B/body-regions -H "$H" | head -c 200; echo
curl -s $B/injury-conditions -H "$H" | head -c 400; echo
ACL=$(docker exec kinetiq-postgres psql -U kinetiq -d kinetiq -tAc "select id from injury_conditions where slug='acl_tear'")
curl -s -w " -> %{http_code}\n" -X POST $B/users/me/injuries -H "$H" -H "$J" -d "{\"condition_id\":$ACL,\"severity\":\"severe\"}"
curl -s -w " -> %{http_code}\n" -X POST $B/users/me/injuries -H "$H" -H "$J" -d "{\"condition_id\":$ACL,\"severity\":\"mild\",\"body_region\":\"knee\"}"
curl -s -w " -> %{http_code}\n" -X POST $B/users/me/injuries -H "$H" -H "$J" -d '{"body_region":"wrist_hand","injury_type":"joint","severity":"mild","muscle_ids":[]}'
curl -s -w " -> %{http_code}\n" -X POST $B/users/me/injuries -H "$H" -H "$J" -d '{"body_region":"left_knee","injury_type":"joint","severity":"mild"}'
curl -s -w " -> %{http_code}\n" $B/body-regions
docker exec kinetiq-postgres psql -U kinetiq -d kinetiq -qc "delete from users where email='live-cat@example.com'"
```
Expected: 14 regions; conditions with muscles and roles; ACL injury **201** with 3 secondary
muscles and `condition.icd11_code` `NC93.62`; ACL + `body_region` **422** naming `body_region`;
wrist injury **201** with `condition: null`, `muscles: []`; `left_knee` **422** on `body_region`;
no token **401**. Stop the server afterwards.

- [ ] **Step 2: Update `docs/02-api-contract.md`**

Replace the body of `## Injuries (drives the rule-based safety filter)` (the 4 endpoint lines and
their notes) with the two request shapes, the locked-field rules (422 naming the field), PATCH
semantics (`muscle_ids` replaces; `[]` clears; condition can't change — delete + re-add) and the
response JSON — exactly as in the spec's **HTTP** section. Add a new section before
`## Exercise Catalog`:
```markdown
## Reference catalog (read-only) 🔒
### `GET /body-regions` — `[{slug, name}]` (14 fixed regions)
### `GET /muscles` — `[{id, name, muscle_group}]` (28)
### `GET /injury-conditions` — `[{id, slug, name, icd11_code|null, body_region, injury_type, muscles: [{id, name, role}]}]`
Conditions are general information, not medical advice (see the conditions-catalog spec for sources).
```

- [ ] **Step 3: Update `docs/01-data-model.md`**

Under `### \`User_Injuries\``: `body_region` is now an FK to `Body_Regions(slug)`; add rows for
`injury_type` (5-value CHECK) and `condition_id` (nullable FK → `Injury_Conditions`). Add short
sections `### \`Body_Regions\``, `### \`Injury_Conditions\` + \`Injury_Condition_Muscles\``
(role `primary|secondary`) and `### \`User_Injury_Muscles\`` (unknown injuries only), each pointing
to migrations 0004/0005 and the spec. Change the `Muscles` seed sentence to list the 28 muscles.
Add `Exercise_Contraindications.body_region` → FK note.

- [ ] **Step 4: Build log, CONTEXT, WORKLOG**

- `docs/phase-2-slice-2-profile-injuries.md`: under `## Step-by-step build log`, add
  `### Stage B extension — Injury conditions catalog (2026-10-21)` summarising: Tasks 1–2 of the July
  plan (injury_type + 0003), the multi-muscle redesign (why), migrations 0004/0005, the 5 tasks and
  the verification results.
- `docs/CONTEXT.md`: in Slice 2 status replace the "Injury/Muscle link: DESIGNED + PLANNED" bullet
  with "Injury conditions catalog: ✅ DONE" (one paragraph: catalog, locked known conditions,
  unknown injuries, 3 endpoints, test count) and set **next = Stage C (#25 frontend forms)**, noting
  the UI must show the not-medical-advice notice. Update the docs map with the new spec + plan.
- `WORKLOG.md`: dated entry at the top (what changed, decisions, verification, next).

- [ ] **Step 5: Commit**
```bash
git add docs/02-api-contract.md docs/01-data-model.md docs/phase-2-slice-2-profile-injuries.md docs/CONTEXT.md WORKLOG.md
git commit -m "docs: injury conditions catalog -- API contract, data model, build log, context"
```
