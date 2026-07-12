#include "adapters/crypto/Argon2PasswordHasher.h"

#include <argon2.h>
#include <openssl/rand.h>

#include <cstring>
#include <stdexcept>
#include <vector>

namespace fitapp::crypto {

namespace {

// Cost parameters: same values already proven to build/run on this toolchain
// by the Slice 0 smoke test (test_auth_deps.cpp). Tunable later without
// affecting existing hashes -- argon2's encoded format embeds the params
// used, so argon2id_verify() always knows how to check an older hash.
constexpr uint32_t kTimeCost = 2;
constexpr uint32_t kMemoryCostKiB = 1 << 16;  // 64 MiB
constexpr uint32_t kParallelism = 1;
constexpr size_t kSaltLength = 16;
constexpr size_t kHashLength = 32;

}  // namespace

std::string Argon2PasswordHasher::hash(const std::string& password) {
    std::vector<unsigned char> salt(kSaltLength);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
        throw std::runtime_error("Argon2PasswordHasher: failed to generate random salt");
    }

    const size_t encodedLen =
        argon2_encodedlen(kTimeCost, kMemoryCostKiB, kParallelism,
                           static_cast<uint32_t>(kSaltLength),
                           static_cast<uint32_t>(kHashLength), Argon2_id);
    std::vector<char> encoded(encodedLen);

    const int rc = argon2id_hash_encoded(kTimeCost, kMemoryCostKiB, kParallelism, password.data(),
                                          password.size(), salt.data(), salt.size(), kHashLength,
                                          encoded.data(), encoded.size());
    if (rc != ARGON2_OK) {
        throw std::runtime_error(std::string("Argon2PasswordHasher: hashing failed: ") +
                                  argon2_error_message(rc));
    }

    // argon2id_hash_encoded null-terminates within the buffer; the buffer
    // itself may be longer than the actual encoded string.
    return std::string(encoded.data(), std::strlen(encoded.data()));
}

bool Argon2PasswordHasher::verify(const std::string& hash, const std::string& password) {
    return argon2id_verify(hash.c_str(), password.data(), password.size()) == ARGON2_OK;
}

}  // namespace fitapp::crypto
