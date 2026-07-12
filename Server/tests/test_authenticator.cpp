// Slice 1 (#17): the authenticator -- a plain function, not Crow middleware,
// because the in-process HttpClient test harness (app.handle_full()) does
// not run middleware. See phase-2-plan.md §8. Independently unit-tested here
// against valid / missing / malformed / invalid / expired tokens, per the
// design note in that doc.

#include "adapters/crypto/JwtTokenService.h"
#include "http/middleware/Authenticator.h"

#include "crow_all.h"

#include <gtest/gtest.h>

#include <chrono>

using fitapp::crypto::JwtTokenService;
using fitapp::http::authenticate;

TEST(AuthenticatorTest, MissingAuthorizationHeader_ReturnsNullopt) {
    JwtTokenService tokens("secret", std::chrono::seconds(3600));
    crow::request req;

    EXPECT_FALSE(authenticate(req, tokens).has_value());
}

TEST(AuthenticatorTest, ValidBearerToken_ReturnsCurrentUser) {
    JwtTokenService tokens("secret", std::chrono::seconds(3600));
    const std::string token = tokens.issue(7);

    crow::request req;
    req.add_header("Authorization", "Bearer " + token);

    const auto user = authenticate(req, tokens);
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->userId, 7);
}

TEST(AuthenticatorTest, HeaderNotBearerScheme_ReturnsNullopt) {
    JwtTokenService tokens("secret", std::chrono::seconds(3600));

    crow::request req;
    req.add_header("Authorization", "Basic sometoken");

    EXPECT_FALSE(authenticate(req, tokens).has_value());
}

TEST(AuthenticatorTest, InvalidToken_ReturnsNullopt) {
    JwtTokenService tokens("secret", std::chrono::seconds(3600));

    crow::request req;
    req.add_header("Authorization", "Bearer garbage-token");

    EXPECT_FALSE(authenticate(req, tokens).has_value());
}

TEST(AuthenticatorTest, ExpiredToken_ReturnsNullopt) {
    JwtTokenService tokens("secret", std::chrono::seconds(-1));
    const std::string token = tokens.issue(7);

    crow::request req;
    req.add_header("Authorization", "Bearer " + token);

    EXPECT_FALSE(authenticate(req, tokens).has_value());
}
