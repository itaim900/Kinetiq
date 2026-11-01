#pragma once

#include "ports/IPasswordHasher.h"

#include <string>

namespace kinetiq::testsupport {

// Hand-written fake for kinetiq::ports::IPasswordHasher (no gmock). Instant and
// deterministic, so service and HTTP tests don't pay argon2's deliberate cost
// (~0.2 s and 64 MiB per hash). It still never stores the plain password as-is
// ("fake$" prefix), and it counts calls so tests can check how often hashing
// and verifying happened. The real adapter is tested in test_password_hasher.cpp.
class FakePasswordHasher : public kinetiq::ports::IPasswordHasher {
public:
    kinetiq::domain::PasswordHash hash(const std::string& password) override {
        ++hashCalls;
        return kinetiq::domain::PasswordHash("fake$" + password);
    }

    bool verify(const kinetiq::domain::PasswordHash& hash, const std::string& password) override {
        ++verifyCalls;
        return hash.encoded() == "fake$" + password;
    }

    int hashCalls = 0;
    int verifyCalls = 0;
};

}  // namespace kinetiq::testsupport
