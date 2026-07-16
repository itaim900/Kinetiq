// Slice 1 (#17): password hasher port + real argon2id adapter. The Slice 0
// smoke test (test_auth_deps.cpp) proved argon2 works on this toolchain;
// this proves the app-facing wrapper is correct (random salt per call,
// round-trips, rejects wrong passwords).

#include <gtest/gtest.h>

#include "adapters/crypto/Argon2PasswordHasher.h"

using kinetiq::crypto::Argon2PasswordHasher;

TEST(Argon2PasswordHasherTest, HashThenVerify_CorrectPassword_Succeeds) {
    Argon2PasswordHasher hasher;
    const std::string hash = hasher.hash("correct horse battery staple");
    EXPECT_TRUE(hasher.verify(hash, "correct horse battery staple"));
}

TEST(Argon2PasswordHasherTest, Verify_WrongPassword_Fails) {
    Argon2PasswordHasher hasher;
    const std::string hash = hasher.hash("correct horse battery staple");
    EXPECT_FALSE(hasher.verify(hash, "wrong password"));
}

// Each call must use a fresh random salt -- otherwise two users with the
// same password would have identical hashes, leaking that fact to anyone
// with DB access.
TEST(Argon2PasswordHasherTest, Hash_UsesADifferentSaltEachTime) {
    Argon2PasswordHasher hasher;
    const std::string hashA = hasher.hash("same password");
    const std::string hashB = hasher.hash("same password");

    EXPECT_NE(hashA, hashB);
    EXPECT_TRUE(hasher.verify(hashA, "same password"));
    EXPECT_TRUE(hasher.verify(hashB, "same password"));
}
