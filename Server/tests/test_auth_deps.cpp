// Slice 0 smoke tests: prove the Phase 2 auth libraries (argon2 + jwt-cpp)
// build, link, and run correctly on this platform. These are toolchain checks,
// not the real auth logic (that arrives in Slice 1 behind ports/services).

#include <gtest/gtest.h>

#include <argon2.h>
#include <jwt-cpp/jwt.h>

#include <string>

// argon2id: hash a password and verify it round-trips (and that a wrong
// password is rejected). Uses modest cost params to keep the test fast.
TEST(AuthDeps, Argon2HashAndVerify) {
    const std::string password = "correct horse battery staple";
    const std::string salt = "0123456789abcdef";  // 16 bytes, test-only
    char encoded[256];

    const int rc = argon2id_hash_encoded(
        /*t_cost=*/2, /*m_cost KiB=*/1 << 16, /*parallelism=*/1,
        password.data(), password.size(),
        salt.data(), salt.size(),
        /*hashlen=*/32, encoded, sizeof(encoded));

    ASSERT_EQ(rc, ARGON2_OK);
    EXPECT_EQ(argon2id_verify(encoded, password.data(), password.size()), ARGON2_OK);
    EXPECT_NE(argon2id_verify(encoded, "wrong password", 14), ARGON2_OK);
}

// jwt-cpp: sign an HS256 token and verify its signature + claims.
TEST(AuthDeps, JwtSignAndVerify) {
    const std::string secret = "test-secret-not-for-production";

    const auto token = jwt::create()
                           .set_issuer("kinetiq")
                           .set_type("JWT")
                           .set_subject("42")
                           .sign(jwt::algorithm::hs256{secret});

    const auto decoded = jwt::decode(token);
    const auto verifier = jwt::verify()
                              .allow_algorithm(jwt::algorithm::hs256{secret})
                              .with_issuer("kinetiq");

    EXPECT_NO_THROW(verifier.verify(decoded));
    EXPECT_EQ(decoded.get_subject(), "42");

    // A token signed with a different secret must fail verification.
    const auto forged = jwt::create()
                            .set_issuer("kinetiq")
                            .set_subject("42")
                            .sign(jwt::algorithm::hs256{"wrong-secret"});
    EXPECT_THROW(verifier.verify(jwt::decode(forged)), std::exception);
}
