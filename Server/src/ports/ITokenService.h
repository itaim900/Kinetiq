#pragma once

#include <optional>
#include <string>

namespace kinetiq::ports {

// Port for issuing/verifying auth tokens. Implementation: JwtTokenService
// (adapters/crypto/, real, HS256). No in-memory fake yet -- introduce one
// only when a service-layer test needs it.
class ITokenService {
public:
    virtual ~ITokenService() = default;

    // Issues a token that identifies `userId`, valid for this service's
    // configured lifetime.
    virtual std::string issue(long long userId) = 0;

    // Returns the user id if `token` is well-formed, correctly signed, and
    // not expired; std::nullopt otherwise (never throws for a bad token --
    // callers should treat nullopt as "unauthenticated", not a bug).
    virtual std::optional<long long> verify(const std::string& token) = 0;
};

}  // namespace kinetiq::ports
