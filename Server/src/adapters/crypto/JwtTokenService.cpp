#include "adapters/crypto/JwtTokenService.h"

#include <jwt-cpp/jwt.h>

#include <exception>

namespace fitapp::crypto {

namespace {
constexpr const char* kIssuer = "fitapp";
}

std::string JwtTokenService::issue(long long userId) {
    const auto now = std::chrono::system_clock::now();
    return jwt::create()
        .set_issuer(kIssuer)
        .set_type("JWT")
        .set_subject(std::to_string(userId))
        .set_issued_at(now)
        .set_expires_at(now + ttl_)
        .sign(jwt::algorithm::hs256{secret_});
}

std::optional<long long> JwtTokenService::verify(const std::string& token) {
    try {
        const auto decoded = jwt::decode(token);
        const auto verifier =
            jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret_}).with_issuer(kIssuer);
        verifier.verify(decoded);  // throws if signature/issuer/exp invalid
        return std::stoll(decoded.get_subject());
    } catch (const std::exception&) {
        // Malformed token, bad signature, wrong issuer, expired, or a
        // non-numeric subject -- all mean "not a valid session," never a
        // reason to crash the request.
        return std::nullopt;
    }
}

}  // namespace fitapp::crypto
