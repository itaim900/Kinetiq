// Slice 2 (#20): HTTP layer for /users/me/injuries CRUD. Drives real routing
// via HttpClient. Registers auth + injury routes; two users prove ownership
// isolation (another user's injury -> 404).

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "adapters/crypto/JwtTokenService.h"
#include "http/controllers/AuthController.h"
#include "http/controllers/InjuryController.h"
#include "support/InMemoryCatalogRepository.h"
#include "support/InMemoryInjuryRepository.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::crypto::JwtTokenService;
using kinetiq::testsupport::InMemoryCatalogRepository;
using kinetiq::testsupport::InMemoryInjuryRepository;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

struct InjuryApi {
    crow::SimpleApp app;
    InMemoryUserRepository users;
    InMemoryInjuryRepository injuries;
    InMemoryCatalogRepository catalog;
    Argon2PasswordHasher hasher;
    JwtTokenService tokens{"test-secret", std::chrono::seconds(3600)};

    InjuryApi() {
        kinetiq::http::registerAuthRoutes(app, users, hasher, tokens);
        kinetiq::http::registerInjuryRoutes(app, injuries, catalog, tokens);
    }
};

template <typename App>
std::string registerAndGetToken(kinetiq::testing::HttpClient<App>& client,
                                const std::string& email) {
    const auto body = crow::json::load(
        client
            .post("/api/v1/auth/register",
                  R"({"email":")" + email + R"(","password":"StrongPass1","display_name":"T"})")
            .body);
    return std::string(body["access_token"].s());
}

}  // namespace

TEST(InjuryControllerTest, Create_ValidInjury_Returns201AndAppearsInList) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-a@example.com");

    const auto created = client.post("/api/v1/users/me/injuries",
                                     R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                                     {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(created.code, 201);

    const auto list = client.get("/api/v1/users/me/injuries", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(list.code, 200);
    const auto body = crow::json::load(list.body);
    ASSERT_TRUE(body);
    ASSERT_EQ(body.size(), 1u);
    EXPECT_EQ(std::string(body[0]["body_region"].s()), "lower_back");
    EXPECT_EQ(std::string(body[0]["injury_type"].s()), "other");
}

TEST(InjuryControllerTest, Create_BadSeverity_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-b@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"body_region":"lower_back","severity":"critical","injury_type":"other"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_Unauthenticated_Returns401) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"body_region":"lower_back","severity":"mild","injury_type":"other"})");
    EXPECT_EQ(r.code, 401);
}

TEST(InjuryControllerTest, Patch_OwnInjury_Returns200) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-c@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"active":false})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
}

TEST(InjuryControllerTest, Patch_OtherUsersInjury_Returns404) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string tokenA = registerAndGetToken(client, "inj-owner@example.com");
    const std::string tokenB = registerAndGetToken(client, "inj-other@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + tokenA}})
            .body);
    const long long id = created["id"].i();

    // User B tries to edit user A's injury.
    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"active":false})", {{"Authorization", "Bearer " + tokenB}});
    EXPECT_EQ(r.code, 404);
}

TEST(InjuryControllerTest, Delete_OwnInjury_Returns204AndGone) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-d@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto del = client.send(crow::HTTPMethod::Delete,
                                 "/api/v1/users/me/injuries/" + std::to_string(id), "",
                                 {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(del.code, 204);

    const auto list = crow::json::load(
        client.get("/api/v1/users/me/injuries", {{"Authorization", "Bearer " + token}}).body);
    EXPECT_EQ(list.size(), 0u);
}

TEST(InjuryControllerTest, Delete_OtherUsersInjury_Returns404) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string tokenA = registerAndGetToken(client, "inj-del-owner@example.com");
    const std::string tokenB = registerAndGetToken(client, "inj-del-other@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"moderate","injury_type":"other"})",
                  {{"Authorization", "Bearer " + tokenA}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Delete,
                               "/api/v1/users/me/injuries/" + std::to_string(id), "",
                               {{"Authorization", "Bearer " + tokenB}});
    EXPECT_EQ(r.code, 404);
}

TEST(InjuryControllerTest, Create_BadInjuryType_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-type@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"body_region":"shin","severity":"mild","injury_type":"sprain"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
}

TEST(InjuryControllerTest, Create_KnownCondition_Returns201WithCatalogData) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-known@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"condition_id":1,"severity":"moderate"})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 201);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(std::string(body["condition"]["name"].s()), "Hamstring strain");
    EXPECT_EQ(std::string(body["condition"]["icd11_code"].s()), "NC76.30");
    EXPECT_EQ(std::string(body["body_region"].s()), "thigh");
    ASSERT_EQ(body["muscles"].size(), 2u);
    EXPECT_EQ(std::string(body["muscles"][0]["name"].s()), "Hamstrings");
    EXPECT_EQ(std::string(body["muscles"][0]["role"].s()), "primary");
}

TEST(InjuryControllerTest, Create_KnownConditionWithLockedField_Returns422NamingField) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-locked@example.com");

    const auto r = client.post("/api/v1/users/me/injuries",
                               R"({"condition_id":1,"severity":"mild","muscle_ids":[2]})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_TRUE(body["errors"].has("muscle_ids"));
}

TEST(InjuryControllerTest, Create_UnknownWithMuscleIds_Returns201Deduplicated) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-muscles@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[2,2,3]})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 201);
    const auto body = crow::json::load(r.body);
    EXPECT_EQ(body["condition"].t(), crow::json::type::Null);
    EXPECT_EQ(body["muscles"].size(), 2u);
}

TEST(InjuryControllerTest, Create_UnknownMuscleId_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-badmuscle@example.com");

    const auto r = client.post(
        "/api/v1/users/me/injuries",
        R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[999]})",
        {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("muscle_ids"));
}

TEST(InjuryControllerTest, Patch_KnownCondition_InjuryType_Returns422) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-patch-known@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries", R"({"condition_id":1,"severity":"mild"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"injury_type":"bone"})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("injury_type"));
}

TEST(InjuryControllerTest, Patch_Unknown_MuscleIdsEmpty_ClearsMuscles) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-clear@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"thigh","injury_type":"muscle","severity":"mild","muscle_ids":[2]})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const long long id = created["id"].i();

    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(id),
                               R"({"muscle_ids":[]})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(crow::json::load(r.body)["muscles"].size(), 0u);
}
