// Slice 1 (#18): HTTP layer for auth. Drives real routing/handlers through
// HttpClient (app.handle_full(), no socket -- see support/HttpClient.h).
// Uses fakes for the repository and real crypto for the hasher/token
// service, matching the app/ service tests.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "support/InMemoryUserRepository.h"

#include <chrono>

using fitapp::crypto::Argon2PasswordHasher;
using fitapp::crypto::JwtTokenService;
using fitapp::testsupport::InMemoryUserRepository;

TEST(AuthControllerTest, Register_ValidRequest_Returns201WithUserAndToken) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r = client.post(
        "/api/v1/auth/register",
        R"({"email":"kate@example.com","password":"StrongPass1","display_name":"Kate"})");

    EXPECT_EQ(r.code, 201);

    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["user"]["email"].s()), "kate@example.com");
    EXPECT_EQ(std::string(body["user"]["display_name"].s()), "Kate");
    EXPECT_FALSE(body["user"].has("password_hash"));
    EXPECT_FALSE(body["user"].has("passwordHash"));
    EXPECT_FALSE(std::string(body["access_token"].s()).empty());
}

TEST(AuthControllerTest, Register_WeakPassword_Returns422) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r = client.post(
        "/api/v1/auth/register",
        R"({"email":"leo@example.com","password":"weak","display_name":"Leo"})");

    EXPECT_EQ(r.code, 422);
    EXPECT_FALSE(users.findByEmail("leo@example.com").has_value());
}

TEST(AuthControllerTest, Register_DuplicateEmail_Returns409) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const std::string requestBody =
        R"({"email":"mona@example.com","password":"StrongPass1","display_name":"Mona"})";
    EXPECT_EQ(client.post("/api/v1/auth/register", requestBody).code, 201);

    const auto r = client.post("/api/v1/auth/register", requestBody);
    EXPECT_EQ(r.code, 409);
}

// Valid JSON, but missing the "password" key entirely -- distinct from
// malformed JSON text (which crow::json::load itself rejects).
TEST(AuthControllerTest, Register_MissingRequiredField_Returns422) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r = client.post("/api/v1/auth/register",
                                R"({"email":"nina@example.com","display_name":"Nina"})");

    EXPECT_EQ(r.code, 422);
}

TEST(AuthControllerTest, Login_CorrectCredentials_Returns200WithUserAndToken) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    client.post("/api/v1/auth/register",
                 R"({"email":"oscar@example.com","password":"StrongPass1","display_name":"Oscar"})");

    const auto r = client.post("/api/v1/auth/login",
                                R"({"email":"oscar@example.com","password":"StrongPass1"})");

    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["user"]["email"].s()), "oscar@example.com");
    EXPECT_FALSE(std::string(body["access_token"].s()).empty());
}

// Wrong password and unknown email must return the identical 401 body --
// see InvalidCredentialsError's comment in app/LoginUser.h.
TEST(AuthControllerTest, Login_WrongPassword_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    client.post("/api/v1/auth/register",
                 R"({"email":"paula@example.com","password":"StrongPass1","display_name":"Paula"})");

    const auto r = client.post("/api/v1/auth/login",
                                R"({"email":"paula@example.com","password":"WrongPass1"})");
    EXPECT_EQ(r.code, 401);
}

TEST(AuthControllerTest, Login_UnknownEmail_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r = client.post("/api/v1/auth/login",
                                R"({"email":"nobody@example.com","password":"WhateverPass1"})");
    EXPECT_EQ(r.code, 401);
}

TEST(AuthControllerTest, Me_ValidToken_Returns200WithUserProfile) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto registerBody = crow::json::load(
        client
            .post("/api/v1/auth/register",
                   R"({"email":"quinn@example.com","password":"StrongPass1","display_name":"Quinn"})")
            .body);
    const std::string token = std::string(registerBody["access_token"].s());

    const auto r =
        client.get("/api/v1/auth/me", {{"Authorization", "Bearer " + token}});

    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["email"].s()), "quinn@example.com");
    EXPECT_EQ(std::string(body["display_name"].s()), "Quinn");
    EXPECT_FALSE(body.has("password_hash"));
}

TEST(AuthControllerTest, Me_MissingToken_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    EXPECT_EQ(client.get("/api/v1/auth/me").code, 401);
}

TEST(AuthControllerTest, Me_InvalidToken_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r =
        client.get("/api/v1/auth/me", {{"Authorization", "Bearer garbage-token"}});
    EXPECT_EQ(r.code, 401);
}

namespace {

// Shared by the change-password tests: register a user and return their
// token, so each test doesn't repeat the same register+extract dance.
// HttpClient is templated on the Crow app type, so this helper is too.
template <typename App>
std::string registerAndGetToken(fitapp::testing::HttpClient<App>& client, const std::string& email,
                                 const std::string& password) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                   R"({"email":")" + email + R"(","password":")" + password +
                       R"(","display_name":"Test User"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(AuthControllerTest, ChangePassword_ValidRequest_Returns200AndNewPasswordWorks) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const std::string token = registerAndGetToken(client, "rita@example.com", "OldPass1");

    const auto r = client.send(
        crow::HTTPMethod::Patch, "/api/v1/auth/password",
        R"({"current_password":"OldPass1","new_password":"NewPass1"})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);

    // Old password no longer works; new one does.
    EXPECT_EQ(client.post("/api/v1/auth/login",
                           R"({"email":"rita@example.com","password":"OldPass1"})")
                  .code,
              401);
    EXPECT_EQ(client.post("/api/v1/auth/login",
                           R"({"email":"rita@example.com","password":"NewPass1"})")
                  .code,
              200);
}

TEST(AuthControllerTest, ChangePassword_WrongCurrentPassword_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const std::string token = registerAndGetToken(client, "sam@example.com", "OldPass1");

    const auto r = client.send(
        crow::HTTPMethod::Patch, "/api/v1/auth/password",
        R"({"current_password":"WrongPass1","new_password":"NewPass1"})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 401);
}

TEST(AuthControllerTest, ChangePassword_WeakNewPassword_Returns422) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const std::string token = registerAndGetToken(client, "tina@example.com", "OldPass1");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/auth/password",
                                R"({"current_password":"OldPass1","new_password":"weak"})",
                                {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(AuthControllerTest, ChangePassword_Unauthenticated_Returns401) {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens("test-secret", std::chrono::seconds(3600));
    fitapp::http::registerAuthRoutes(app, users, hasher, tokens);
    fitapp::testing::HttpClient client(app);

    const auto r =
        client.send(crow::HTTPMethod::Patch, "/api/v1/auth/password",
                     R"({"current_password":"Whatever1","new_password":"NewPass1"})");
    EXPECT_EQ(r.code, 401);
}
