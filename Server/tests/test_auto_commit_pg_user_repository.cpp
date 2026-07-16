// Slice 1 (#18): AutoCommitPgUserRepository -- the repository actually wired
// into main.cpp. Unlike every other DbTest-based test in this suite, it
// deliberately opens its OWN connections (see its header for why), so it
// can't rely on DbTest's per-test transaction rollback for cleanup -- this
// test cleans up after itself explicitly instead.

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgUserRepository.h"
#include "domain/User.h"

#include <pqxx/pqxx>

using fitapp::db::AutoCommitPgUserRepository;
using fitapp::domain::User;

namespace {

void deleteTestUser(const std::string& email) {
    pqxx::connection conn(fitapp::testing::testConnString());
    pqxx::work tx(conn);
    tx.exec("DELETE FROM users WHERE email = " + tx.quote(email));
    tx.commit();
}

}  // namespace

TEST(AutoCommitPgUserRepositoryTest, SaveThenFindByEmail_PersistsAcrossSeparateConnections) {
    if (!fitapp::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }

    AutoCommitPgUserRepository repo(fitapp::testing::testConnString());
    const std::string email = "auto-commit-test@example.com";

    User user;
    user.email = email;
    user.passwordHash = "hashed";
    user.displayName = "Auto Commit";
    const User saved = repo.save(user);

    // A fresh call opens a NEW connection -- if this finds the row, save()
    // genuinely committed rather than leaving it in an uncommitted
    // transaction only visible to itself.
    const auto found = repo.findByEmail(email);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);

    deleteTestUser(email);
}

TEST(AutoCommitPgUserRepositoryTest, Update_PersistsAcrossSeparateConnections) {
    if (!fitapp::testing::ensureTestDatabaseReady()) {
        GTEST_SKIP() << "no test database reachable";
    }

    AutoCommitPgUserRepository repo(fitapp::testing::testConnString());
    const std::string email = "auto-commit-update-test@example.com";

    User user;
    user.email = email;
    user.passwordHash = "old-hash";
    const User saved = repo.save(user);

    User modified = saved;
    modified.passwordHash = "new-hash";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->passwordHash, "new-hash");

    deleteTestUser(email);
}
