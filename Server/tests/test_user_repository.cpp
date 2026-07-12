// Slice 1: IUserRepository port, exercised against the in-memory fake
// (tests/support/InMemoryUserRepository.h). The real adapter (PgUserRepository,
// adapters/db/) must satisfy the exact same contract -- these cases double as
// the spec for it.

#include <gtest/gtest.h>

#include "domain/User.h"
#include "ports/IUserRepository.h"
#include "support/InMemoryUserRepository.h"

using fitapp::domain::User;
using fitapp::ports::DuplicateEmailError;
using fitapp::testsupport::InMemoryUserRepository;

TEST(InMemoryUserRepositoryTest, SaveThenFindByEmail_ReturnsUser) {
    InMemoryUserRepository repo;
    User user;
    user.email = "alice@example.com";
    user.passwordHash = "hashed";
    user.displayName = "Alice";

    const User saved = repo.save(user);
    EXPECT_GT(saved.id, 0);

    const auto found = repo.findByEmail("alice@example.com");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, saved.id);
    EXPECT_EQ(found->displayName, "Alice");
}

TEST(InMemoryUserRepositoryTest, FindByEmail_UnknownEmail_ReturnsNullopt) {
    InMemoryUserRepository repo;
    EXPECT_FALSE(repo.findByEmail("nobody@example.com").has_value());
}

// Users.email is `citext` in Postgres (case-insensitive) -- the fake must
// match, or tests written against it would pass here and fail for real.
TEST(InMemoryUserRepositoryTest, FindByEmail_IsCaseInsensitive) {
    InMemoryUserRepository repo;
    User user;
    user.email = "Alice@Example.com";
    repo.save(user);

    EXPECT_TRUE(repo.findByEmail("alice@example.com").has_value());
    EXPECT_TRUE(repo.findByEmail("ALICE@EXAMPLE.COM").has_value());
}

TEST(InMemoryUserRepositoryTest, Save_DuplicateEmailCaseInsensitive_Throws) {
    InMemoryUserRepository repo;
    User first;
    first.email = "alice@example.com";
    repo.save(first);

    User second;
    second.email = "ALICE@EXAMPLE.COM";
    EXPECT_THROW(repo.save(second), DuplicateEmailError);
}

TEST(InMemoryUserRepositoryTest, Save_AssignsDistinctIncrementingIds) {
    InMemoryUserRepository repo;
    User a;
    a.email = "a@example.com";
    User b;
    b.email = "b@example.com";

    const User savedA = repo.save(a);
    const User savedB = repo.save(b);
    EXPECT_NE(savedA.id, savedB.id);
}
