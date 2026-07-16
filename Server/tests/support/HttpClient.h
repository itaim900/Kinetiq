#pragma once

#include "crow_all.h"

#include <string>
#include <utility>
#include <vector>

namespace kinetiq::testing {

struct TestResponse {
    int code = 0;
    std::string body;
};

// Sends requests through a Crow app IN-PROCESS (no network socket) via
// app.handle_full(), exercising the real routing + handlers + DTO serialization.
//
// IMPORTANT: Crow middleware (global OR route-local) is NOT executed by
// handle_full() — it is wired up by the socket Connection, and handle_full even
// dereferences an uninitialized middleware context if a route declares one. So
// keep tested routes middleware-free and implement auth as an *authenticator the
// handler calls* (unit-tested separately). This keeps the whole request path
// testable here without sockets. See docs/phase-2-plan.md.
template <typename App>
class HttpClient {
public:
    using Headers = std::vector<std::pair<std::string, std::string>>;

    explicit HttpClient(App& app) : app_(app) { app_.validate(); }

    TestResponse send(crow::HTTPMethod method, const std::string& target,
                      const std::string& body = "", const Headers& headers = {}) {
        crow::request req;
        req.method = method;
        req.raw_url = target;
        req.url = pathOf(target);
        req.body = body;
        for (const auto& [key, value] : headers) {
            req.add_header(key, value);
        }
        crow::response res;
        app_.handle_full(req, res);
        return {res.code, res.body};
    }

    TestResponse get(const std::string& target, const Headers& headers = {}) {
        return send(crow::HTTPMethod::Get, target, "", headers);
    }

    TestResponse post(const std::string& target, const std::string& body,
                      const Headers& headers = {}) {
        return send(crow::HTTPMethod::Post, target, body, headers);
    }

private:
    // Strip a query string; handle_full matches routes on the path only.
    static std::string pathOf(const std::string& target) {
        const auto q = target.find('?');
        return q == std::string::npos ? target : target.substr(0, q);
    }

    App& app_;
};

}  // namespace kinetiq::testing
