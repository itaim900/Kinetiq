// Catalog endpoints: GET /body-regions, /muscles, /injury-conditions (all 🔒).

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/CatalogController.h"
#include "support/InMemoryCatalogRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryCatalogRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct CatalogApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryCatalogRepository catalog;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    CatalogApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerCatalogRoutes(app, catalog, tokens);
    }
};

template <typename App>
std::string tokenFor(kinetiq::testing::HttpClient<App>& client) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":"cat@example.com","password":"StrongPass1","display_name":"C"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(CatalogControllerTest, GetBodyRegions_Returns200AndList) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.get("/api/v1/body-regions",
                              {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(std::string(body[0]["slug"].s()), "knee");
    EXPECT_EQ(std::string(body[0]["name"].s()), "Knee");
}

TEST(CatalogControllerTest, GetMuscles_Returns200AndList) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r =
        client.get("/api/v1/muscles", {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_EQ(body.size(), 4u);
    EXPECT_EQ(std::string(body[0]["name"].s()), "Hamstrings");
    EXPECT_EQ(std::string(body[0]["muscle_group"].s()), "legs");
}

TEST(CatalogControllerTest, GetInjuryConditions_IncludesMusclesAndRoles) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.get("/api/v1/injury-conditions",
                              {{"Authorization", "Bearer " + tokenFor(client)}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(std::string(body[0]["name"].s()), "Hamstring strain");
    EXPECT_EQ(std::string(body[0]["icd11_code"].s()), "NC76.30");
    EXPECT_EQ(std::string(body[0]["body_region"].s()), "thigh");
    ASSERT_EQ(body[0]["muscles"].size(), 2u);
    EXPECT_EQ(std::string(body[0]["muscles"][0]["role"].s()), "primary");
}

TEST(CatalogControllerTest, Unauthenticated_Returns401) {
    CatalogApi api;
    kinetiq::testing::HttpClient client(api.app);
    for (const char* url : {"/api/v1/body-regions", "/api/v1/muscles", "/api/v1/injury-conditions"}) {
        EXPECT_EQ(client.get(url).code, 401) << url;
    }
}
