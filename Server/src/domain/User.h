#pragma once

#include <string>

namespace fitapp::domain {

// Mirrors the subset of the Users table that auth needs. Deliberately not the
// full profile (biometrics etc. arrive in Slice 2) -- keep this narrow.
struct User {
    long long id = 0;
    std::string email;
    std::string passwordHash;
    std::string displayName;
};

}  // namespace fitapp::domain
