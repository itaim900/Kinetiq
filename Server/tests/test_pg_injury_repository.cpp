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
