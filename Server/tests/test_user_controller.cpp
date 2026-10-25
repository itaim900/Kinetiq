// Slice 2 (#19): HTTP layer for GET/PATCH /users/me. Drives real routing/
// handlers through HttpClient (app.handle_full(), no socket). Registers the
// auth routes too, so tests obtain a real token via register.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/UserController.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

// Wire an app with auth + user routes over one shared fake repo.
struct UserApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    UserApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerUserRoutes(app, users, tokens);
    }
};

template <typename App>
std::string registerAndGetToken(kinetiq::testing::HttpClient<App>& client,
                                const std::string& email) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":")" + email + R"(","password":"StrongPass1","display_name":"Test"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(UserControllerTest, GetMe_ValidToken_Returns200WithProfile) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "gm@example.com");

    const auto r = client.get("/api/v1/users/me", {{"Authorization", "Bearer " + token}});

    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["email"].s()), "gm@example.com");
    EXPECT_EQ(std::string(body["unit_preference"].s()), "metric");  // DB default
    EXPECT_FALSE(body.has("password_hash"));
}

TEST(UserControllerTest, GetMe_MissingToken_Returns401) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    EXPECT_EQ(client.get("/api/v1/users/me").code, 401);
}

TEST(UserControllerTest, PatchMe_PartialUpdate_Returns200AndPersists) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "pm@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"weight_kg":74.5,"experience_level":"intermediate"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);

    const auto after =
        crow::json::load(client.get("/api/v1/users/me", {{"Authorization", "Bearer " + token}}).body);
    ASSERT_TRUE(after);
    EXPECT_DOUBLE_EQ(after["weight_kg"].d(), 74.5);
    EXPECT_EQ(std::string(after["experience_level"].s()), "intermediate");
}

TEST(UserControllerTest, PatchMe_InvalidWeight_Returns422WithFieldError) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "iw@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"weight_kg":-3})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_TRUE(body["errors"].has("weight_kg"));
}

TEST(UserControllerTest, PatchMe_BadExperienceLevel_Returns422) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "be@example.com");

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"experience_level":"expert"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(UserControllerTest, PatchMe_MissingToken_Returns401) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":70})");
    EXPECT_EQ(r.code, 401);
}

TEST(UserControllerTest, PatchMe_NullClearsField_Returns200WithNull) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "clear@example.com");
    client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":80})",
                {{"Authorization", "Bearer " + token}});

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":null})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(body["weight_kg"].t(), crow::json::type::Null);
}

TEST(UserControllerTest, PatchMe_NullUnitPreference_Returns422) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "clear-unit@example.com");
    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"unit_preference":null})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("unit_preference"));
}
