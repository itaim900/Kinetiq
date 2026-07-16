// Slice 1 (#18): LoginUser. Unlike RegisterUser's separable checks (weak
// password vs. duplicate email), "find the user" and "check the password"
// are one cohesive operation here -- so happy path and the failure case are
// built together rather than as two loops.

#include <gtest/gtest.h>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "app/LoginUser.h"
#include "domain/User.h"
#include "support/InMemoryUserRepository.h"

#include <chrono>

using fitapp::app::InvalidCredentialsError;
using fitapp::app::LoginUser;
using fitapp::app::LoginUserRequest;
using fitapp::crypto::Argon2PasswordHasher;
using fitapp::crypto::JwtTokenService;
using fitapp::domain::User;
using fitapp::testsupport::InMemoryUserRepository;

namespace {

// LoginUser verifies against a genuine hash, not a plaintext stand-in, so
// every test needs a properly-hashed seed user.
User seedUser(InMemoryUserRepository& users, Argon2PasswordHasher& hasher,
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
    Argon2PasswordHasher hasher;
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
    Argon2PasswordHasher hasher;
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
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    LoginUser loginUser(users, hasher, tokens);

    const LoginUserRequest request{"nobody@example.com", "WhateverPassword1"};
    EXPECT_THROW(loginUser.execute(request), InvalidCredentialsError);
}
