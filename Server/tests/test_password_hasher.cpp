// Slice 1 (#17): password hasher port + real argon2id adapter. The Slice 0
// smoke test (test_auth_deps.cpp) proved argon2 works on this toolchain;
// this proves the app-facing wrapper is correct (random salt per call,
// round-trips, rejects wrong passwords).

#include <gtest/gtest.h>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "domain/PasswordHash.h"

#include <string>
#include <type_traits>

using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::domain::PasswordHash;

// The point of the PasswordHash type: a password and a hash can't be mixed up
// silently. These are checked by the compiler, not at run time.
static_assert(!std::is_convertible_v<std::string, PasswordHash>,
              "a plain string must not turn into a PasswordHash by accident");
static_assert(!std::is_convertible_v<PasswordHash, std::string>,
              "a PasswordHash must not be passed where a password is expected");

TEST(Argon2PasswordHasherTest, HashThenVerify_CorrectPassword_Succeeds) {
    Argon2PasswordHasher hasher;
    const PasswordHash hash = hasher.hash("correct horse battery staple");
    EXPECT_TRUE(hasher.verify(hash, "correct horse battery staple"));
}

TEST(Argon2PasswordHasherTest, Verify_WrongPassword_Fails) {
    Argon2PasswordHasher hasher;
    const PasswordHash hash = hasher.hash("correct horse battery staple");
    EXPECT_FALSE(hasher.verify(hash, "wrong password"));
}

// Each call must use a fresh random salt -- otherwise two users with the
// same password would have identical hashes, leaking that fact to anyone
// with DB access.
TEST(Argon2PasswordHasherTest, Hash_UsesADifferentSaltEachTime) {
    Argon2PasswordHasher hasher;
    const PasswordHash hashA = hasher.hash("same password");
    const PasswordHash hashB = hasher.hash("same password");

    EXPECT_NE(hashA, hashB);
    EXPECT_TRUE(hasher.verify(hashA, "same password"));
    EXPECT_TRUE(hasher.verify(hashB, "same password"));
}

// The dummy hash LoginUser verifies against for unknown emails must be a real
// argon2 hash (so it costs the same as a real one) and computed only once.
TEST(Argon2PasswordHasherTest, DummyHash_IsARealArgon2HashComputedOnce) {
    Argon2PasswordHasher hasher;
    const PasswordHash& first = hasher.dummyHash();
    EXPECT_EQ(first.encoded().rfind("$argon2id$", 0), 0u);  // starts with
    EXPECT_EQ(&first, &hasher.dummyHash());                 // same object
    EXPECT_FALSE(hasher.verify(first, "StrongPass1"));
}
