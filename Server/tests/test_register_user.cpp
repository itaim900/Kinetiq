// Slice 1 (#18): RegisterUser -- the first app/ service, orchestrating what
// Slice 1 built so far (repository, hasher, token service) for the first
// time. Tested against the in-memory fake repository (no DB needed) and
// FakePasswordHasher (argon2 is slow on purpose; the real hasher has its own
// tests in test_password_hasher.cpp), with the real token service.

#include <gtest/gtest.h>

#include "support/FakePasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "app/RegisterUser.h"
#include "domain/PasswordPolicy.h"
#include "ports/IUserRepository.h"
#include "support/InMemoryUserRepository.h"

#include <chrono>

using kinetiq::app::RegisterUser;
using kinetiq::app::RegisterUserRequest;
using kinetiq::testsupport::FakePasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::ports::DuplicateEmailError;
using kinetiq::testsupport::InMemoryUserRepository;

TEST(RegisterUserTest, ValidRequest_CreatesUserAndReturnsToken) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    RegisterUser registerUser(users, hasher, tokens);

    const RegisterUserRequest request{"alice@example.com", "StrongPass1", "Alice"};
    const auto result = registerUser.execute(request);

    EXPECT_GT(result.user.id, 0);
    EXPECT_EQ(result.user.email, "alice@example.com");
    EXPECT_EQ(result.user.displayName, "Alice");
    EXPECT_FALSE(result.accessToken.empty());

    // The stored hash must never be the plaintext password.
    EXPECT_NE(result.user.passwordHash.encoded(), "StrongPass1");

    // The issued token should verify back to exactly this user's id.
    const auto verifiedUserId = tokens.verify(result.accessToken);
    ASSERT_TRUE(verifiedUserId.has_value());
    EXPECT_EQ(*verifiedUserId, result.user.id);
}

// Weak password must be rejected before anything else happens -- no
// hashing, no repository write. "short" is 5 chars, well under the min 8.
TEST(RegisterUserTest, WeakPassword_ThrowsWithoutTouchingRepositoryOrHasher) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    RegisterUser registerUser(users, hasher, tokens);

    const RegisterUserRequest request{"bob@example.com", "short", "Bob"};

    EXPECT_THROW(registerUser.execute(request), kinetiq::domain::WeakPasswordError);
    EXPECT_FALSE(users.findByEmail("bob@example.com").has_value());
}

// DuplicateEmailError is thrown by the repository (see test_user_repository.cpp
// for the case-insensitivity behind it) -- this proves RegisterUser lets it
// propagate untouched rather than swallowing or wrapping it.
TEST(RegisterUserTest, DuplicateEmail_Throws) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    RegisterUser registerUser(users, hasher, tokens);

    registerUser.execute({"carol@example.com", "StrongPass1", "Carol"});

    const RegisterUserRequest duplicate{"carol@example.com", "AnotherPass1", "Someone Else"};
    EXPECT_THROW(registerUser.execute(duplicate), DuplicateEmailError);
}
