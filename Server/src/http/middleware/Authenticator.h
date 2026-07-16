#pragma once

#include "ports/ITokenService.h"

#include "crow_all.h"

#include <optional>
#include <string>
#include <string_view>

namespace kinetiq::http {

// The currently authenticated user, resolved from a request's token.
struct CurrentUser {
    long long userId = 0;
};

// Reads `Authorization: Bearer <token>` and verifies it via `tokens`.
// Returns std::nullopt for a missing header, a scheme other than Bearer, or
// a malformed/invalid/expired token -- callers should respond 401 in that
// case.
//
// Deliberately a plain function, NOT Crow middleware: the in-process
// HttpClient test harness (app.handle_full()) does not run middleware, so
// middleware-based auth would be untestable without a real socket. Protected
// handlers call this directly instead. See phase-2-plan.md §8.
inline std::optional<CurrentUser> authenticate(const crow::request& req,
                                                kinetiq::ports::ITokenService& tokens) {
    constexpr std::string_view kPrefix = "Bearer ";
    const std::string header = req.get_header_value("Authorization");

    if (header.size() <= kPrefix.size() || header.compare(0, kPrefix.size(), kPrefix) != 0) {
        return std::nullopt;
    }

    const std::string token = header.substr(kPrefix.size());
    const auto userId = tokens.verify(token);
    if (!userId.has_value()) return std::nullopt;

    return CurrentUser{*userId};
}

}  // namespace kinetiq::http
