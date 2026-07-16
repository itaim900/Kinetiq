// Slice 1 (#17): token service port + real JWT (HS256) adapter. The Slice 0
// smoke test (test_auth_deps.cpp) proved jwt-cpp works on this toolchain;
// this proves the app-facing wrapper is correct (round-trips the user id,
// rejects a wrong-secret or expired token).

#include <gtest/gtest.h>

#include "adapters/crypto/JwtTokenService.h"

#include <chrono>

using kinetiq::crypto::JwtTokenService;

TEST(JwtTokenServiceTest, IssueThenVerify_ReturnsSameUserId) {
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));

    const std::string token = tokens.issue(42);
    const auto userId = tokens.verify(token);

    ASSERT_TRUE(userId.has_value());
    EXPECT_EQ(*userId, 42);
}

TEST(JwtTokenServiceTest, Verify_WrongSecret_ReturnsNullopt) {
    JwtTokenService issuer("secret-a", std::chrono::seconds(3600));
    JwtTokenService verifier("secret-b", std::chrono::seconds(3600));

    const std::string token = issuer.issue(42);
    EXPECT_FALSE(verifier.verify(token).has_value());
}

TEST(JwtTokenServiceTest, Verify_MalformedToken_ReturnsNullopt) {
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    EXPECT_FALSE(tokens.verify("not-a-real-token").has_value());
}

// Negative TTL -> already expired the moment it's issued. Lets the test
// assert expiry synchronously instead of sleeping.
TEST(JwtTokenServiceTest, Verify_ExpiredToken_ReturnsNullopt) {
    JwtTokenService tokens("test-secret", std::chrono::seconds(-1));

    const std::string token = tokens.issue(42);
    EXPECT_FALSE(tokens.verify(token).has_value());
}
