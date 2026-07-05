// Smoke test for the in-process HTTP harness (HttpClient). Proves we can drive a
// Crow app through app.handle_full() — routing, verbs, status codes, request
// body, and request headers — without opening a socket. Slice 1's auth/profile
// endpoint tests build on this.

#include "support/HttpClient.h"

#include <gtest/gtest.h>

#include <string>

namespace {

// Defines a few throwaway routes on a real Crow app (no middleware — see the
// note in HttpClient.h about why handle_full can't run middleware).
void defineRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/ping")([] { return std::string("pong"); });

    CROW_ROUTE(app, "/echo")
        .methods(crow::HTTPMethod::Post)([](const crow::request& req) {
            return crow::response(201, req.body);
        });

    // Reads a request header — mirrors how a handler-level authenticator would
    // inspect the Authorization header.
    CROW_ROUTE(app, "/whoami")([](const crow::request& req) {
        const std::string auth = req.get_header_value("Authorization");
        if (auth.empty()) return crow::response(401, "missing");
        return crow::response(200, auth);
    });
}

}  // namespace

TEST(HttpHarness, RoutesGetRequest) {
    crow::SimpleApp app;
    defineRoutes(app);
    fitapp::testing::HttpClient client(app);

    const auto r = client.get("/ping");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body, "pong");
}

TEST(HttpHarness, PostReturnsBodyAndStatus) {
    crow::SimpleApp app;
    defineRoutes(app);
    fitapp::testing::HttpClient client(app);

    const auto r = client.post("/echo", "hello world");
    EXPECT_EQ(r.code, 201);
    EXPECT_EQ(r.body, "hello world");
}

TEST(HttpHarness, PropagatesRequestHeaders) {
    crow::SimpleApp app;
    defineRoutes(app);
    fitapp::testing::HttpClient client(app);

    EXPECT_EQ(client.get("/whoami").code, 401);

    const auto r = client.get("/whoami", {{"Authorization", "Bearer xyz"}});
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body, "Bearer xyz");
}

TEST(HttpHarness, UnknownRouteIs404) {
    crow::SimpleApp app;
    defineRoutes(app);
    fitapp::testing::HttpClient client(app);

    EXPECT_EQ(client.get("/does-not-exist").code, 404);
}
