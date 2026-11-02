#pragma once

#include "domain/PasswordHash.h"

#include <string>

namespace kinetiq::ports {

// Port for password hashing. Implementation: Argon2PasswordHasher
// (adapters/crypto/, real). No in-memory fake yet -- introduce one only when
// a service-layer test actually needs to avoid argon2's deliberate slowness
// (see Server/tests/test_auth_deps.cpp: ~1s per hash).
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    // Returns a self-contained encoded hash (algorithm params + salt + hash
    // all embedded) -- safe to store directly in Users.password_hash.
    virtual kinetiq::domain::PasswordHash hash(const std::string& password) = 0;

    // True if `password`, when hashed, matches `hash`.
    virtual bool verify(const kinetiq::domain::PasswordHash& hash,
                        const std::string& password) = 0;

    // A valid hash of a fixed secret, computed once. LoginUser verifies
    // against it when the email is unknown, so "no such user" costs the same
    // time as "wrong password" and response timing can't reveal which emails
    // are registered.
    virtual const kinetiq::domain::PasswordHash& dummyHash() = 0;
};

}  // namespace kinetiq::ports
