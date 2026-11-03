#pragma once

#include "ports/IPasswordHasher.h"

#include <string>

namespace kinetiq::crypto {

// Real argon2id-backed password hasher. A fresh random salt (via OpenSSL's
// RAND_bytes) is generated on every hash() call and embedded in the returned
// encoded string, so two identical passwords never produce the same hash.
class Argon2PasswordHasher : public kinetiq::ports::IPasswordHasher {
public:
    kinetiq::domain::PasswordHash hash(const std::string& password) override;
    bool verify(const kinetiq::domain::PasswordHash& hash, const std::string& password) override;
    const kinetiq::domain::PasswordHash& dummyHash() override;
};

}  // namespace kinetiq::crypto
