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
    i.injuryType = "other";
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
    repo.create(make(b, "knee", "severe"));

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
