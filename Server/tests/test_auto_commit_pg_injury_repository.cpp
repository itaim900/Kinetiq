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
