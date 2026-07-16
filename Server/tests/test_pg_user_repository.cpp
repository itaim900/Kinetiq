// Slice 1: PgUserRepository -- the real Postgres-backed implementation of
// IUserRepository. Exercises the exact same contract as InMemoryUserRepository
// (see test_user_repository.cpp), against a real database, using DbTest's
// per-test transaction rollback for isolation.

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgUserRepository.h"
#include "domain/User.h"
#include "ports/IUserRepository.h"

using kinetiq::db::PgUserRepository;
using kinetiq::domain::User;
using kinetiq::ports::DuplicateEmailError;

TEST_F(DbTest, PgUserRepository_SaveThenFindByEmail_ReturnsUser) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "pg-alice@example.com";
    user.passwordHash = "hashed";
    user.displayName = "Alice";

    const User saved = repo.save(user);
    EXPECT_GT(saved.id, 0);

    const auto found = repo.findByEmail("pg-alice@example.com");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);
    EXPECT_EQ(found->displayName, "Alice");
    EXPECT_EQ(found->passwordHash, "hashed");
}

TEST_F(DbTest, PgUserRepository_FindByEmail_UnknownEmail_ReturnsNullopt) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    EXPECT_FALSE(repo.findByEmail("pg-nobody@example.com").has_value());
}

// Users.email is `citext` in Postgres -- prove the real adapter honors that,
// same as the fake does.
TEST_F(DbTest, PgUserRepository_FindByEmail_IsCaseInsensitive) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "Pg-Bob@Example.com";
    user.passwordHash = "hashed";
    repo.save(user);

    EXPECT_TRUE(repo.findByEmail("pg-bob@example.com").has_value());
    EXPECT_TRUE(repo.findByEmail("PG-BOB@EXAMPLE.COM").has_value());
}

TEST_F(DbTest, PgUserRepository_Save_DuplicateEmailCaseInsensitive_Throws) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User first;
    first.email = "pg-carol@example.com";
    first.passwordHash = "hashed";
    repo.save(first);

    User second;
    second.email = "PG-CAROL@EXAMPLE.COM";
    second.passwordHash = "hashed2";
    EXPECT_THROW(repo.save(second), DuplicateEmailError);
}

TEST_F(DbTest, PgUserRepository_Save_AssignsDistinctIncrementingIds) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User a;
    a.email = "pg-dan@example.com";
    a.passwordHash = "x";
    User b;
    b.email = "pg-erin@example.com";
    b.passwordHash = "x";

    const User savedA = repo.save(a);
    const User savedB = repo.save(b);
    EXPECT_NE(savedA.id, savedB.id);
}

// Slice 1 (#18): ChangePassword needs to look up the authenticated user by
// id (from the JWT's subject claim), not by email.
TEST_F(DbTest, PgUserRepository_FindById_ReturnsUser) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "pg-frank@example.com";
    user.passwordHash = "hashed";
    user.displayName = "Frank";
    const User saved = repo.save(user);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->email, "pg-frank@example.com");
    EXPECT_EQ(found->displayName, "Frank");
}

TEST_F(DbTest, PgUserRepository_FindById_UnknownId_ReturnsNullopt) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    EXPECT_FALSE(repo.findById(999999999).has_value());
}

// Slice 1 (#18): ChangePassword persists a new password_hash via update().
TEST_F(DbTest, PgUserRepository_Update_ChangesStoredFields) {
    if (!available()) GTEST_SKIP() << "no test database reachable";

    PgUserRepository repo(tx());
    User user;
    user.email = "pg-grace@example.com";
    user.passwordHash = "old-hash";
    const User saved = repo.save(user);

    User modified = saved;
    modified.passwordHash = "new-hash";
    repo.update(modified);

    const auto found = repo.findById(saved.id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->passwordHash, "new-hash");
}
