// Slice 1 (#18): LoginUser. Unlike RegisterUser's separable checks (weak
// password vs. duplicate email), "find the user" and "check the password"
// are one cohesive operation here -- so happy path and the failure case are
// built together rather than as two loops.

#include <gtest/gtest.h>

#include "support/FakePasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "app/LoginUser.h"
#include "domain/User.h"
#include "support/InMemoryUserRepository.h"

#include <chrono>

using kinetiq::app::InvalidCredentialsError;
using kinetiq::app::LoginUser;
using kinetiq::app::LoginUserRequest;
using kinetiq::testsupport::FakePasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::domain::User;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

// LoginUser verifies against a stored hash, so every test seeds a user whose
// hash came from the same (fake) hasher.
User seedUser(InMemoryUserRepository& users, FakePasswordHasher& hasher,
              const std::string& email, const std::string& password) {
    User user;
    user.email = email;
    user.displayName = "Seed User";
    user.passwordHash = hasher.hash(password);
    return users.save(user);
}

}  // namespace

TEST(LoginUserTest, CorrectCredentials_ReturnsUserAndToken) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    const User seeded = seedUser(users, hasher, "dave@example.com", "StrongPass1");

    const LoginUserRequest request{"dave@example.com", "StrongPass1"};
    const auto result = loginUser.execute(request);

    EXPECT_EQ(result.user.id, seeded.id);
    EXPECT_EQ(result.user.email, "dave@example.com");
    EXPECT_FALSE(result.accessToken.empty());

    const auto verifiedUserId = tokens.verify(result.accessToken);
    ASSERT_TRUE(verifiedUserId.has_value());
    EXPECT_EQ(*verifiedUserId, seeded.id);
}

TEST(LoginUserTest, WrongPassword_ThrowsInvalidCredentials) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    seedUser(users, hasher, "erin@example.com", "StrongPass1");

    const LoginUserRequest request{"erin@example.com", "WrongPassword1"};
    EXPECT_THROW(loginUser.execute(request), InvalidCredentialsError);
}

// Deliberately the SAME exception type as wrong password -- see
// InvalidCredentialsError's comment in app/LoginUser.h.
TEST(LoginUserTest, UnknownEmail_ThrowsInvalidCredentials) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    const LoginUserRequest request{"nobody@example.com", "WhateverPassword1"};
    EXPECT_THROW(loginUser.execute(request), InvalidCredentialsError);
}

// Timing-leak fix: both login failures run exactly one (slow, in production)
// verify, so response time can't reveal whether an email is registered.
TEST(LoginUserTest, UnknownEmail_StillVerifiesOnce_SoItTakesAsLongAsAWrongPassword) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    EXPECT_THROW(loginUser.execute({"nobody@example.com", "Whatever1"}), InvalidCredentialsError);
    EXPECT_EQ(hasher.verifyCalls, 1);
}

TEST(LoginUserTest, WrongPassword_VerifiesOnce) {
    InMemoryUserRepository users;
    FakePasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);
    seedUser(users, hasher, "erin@example.com", "RightPass1");

    EXPECT_THROW(loginUser.execute({"erin@example.com", "WrongPass1"}), InvalidCredentialsError);
    EXPECT_EQ(hasher.verifyCalls, 1);
}
