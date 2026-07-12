#pragma once

#include "ports/ITokenService.h"

#include <chrono>
#include <string>

namespace fitapp::crypto {

// Real HS256 JWT-backed token service. `secret` signs/verifies tokens (must
// be kept out of source control in production -- see JWT_SECRET in main.cpp
// once #17 wires this up there). `ttl` is how long an issued token stays
// valid; tests use a short/negative ttl to exercise expiry without sleeping.
class JwtTokenService : public fitapp::ports::ITokenService {
public:
    JwtTokenService(std::string secret, std::chrono::seconds ttl)
        : secret_(std::move(secret)), ttl_(ttl) {}

    std::string issue(long long userId) override;
    std::optional<long long> verify(const std::string& token) override;

private:
    std::string secret_;
    std::chrono::seconds ttl_;
};

}  // namespace fitapp::crypto
