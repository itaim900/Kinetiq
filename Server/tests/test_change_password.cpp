// Slice 1 (#18): ChangePassword -- change password while already
// authenticated (PATCH /auth/password). No email needed, unlike the
// deferred "forgot password" reset flow.

#include <gtest/gtest.h>

#include "adapters/crypto/Argon2PasswordHasher.h"
#include "app/ChangePassword.h"
#include "app/LoginUser.h"
#include "domain/PasswordPolicy.h"
#include "domain/User.h"
#include "support/InMemoryUserRepository.h"

using kinetiq::app::ChangePassword;
using kinetiq::app::ChangePasswordRequest;
using kinetiq::app::InvalidCredentialsError;
using kinetiq::crypto::Argon2PasswordHasher;
using kinetiq::domain::User;
using kinetiq::testsupport::InMemoryUserRepository;

namespace {

User seedUser(InMemoryUserRepository& users, Argon2PasswordHasher& hasher,
              const std::string& email, const std::string& password) {
    User user;
    user.email = email;
    user.passwordHash = hasher.hash(password);
    return users.save(user);
}

}  // namespace

TEST(ChangePasswordTest, CorrectCurrentPassword_UpdatesHashSoNewPasswordVerifies) {
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    ChangePassword changePassword(users, hasher);

    const User seeded = seedUser(users, hasher, "henry@example.com", "OldPass1");

    changePassword.execute({seeded.id, "OldPass1", "NewPass1"});

    const auto updated = users.findById(seeded.id);
    ASSERT_TRUE(updated.has_value());
    EXPECT_TRUE(hasher.verify(updated->passwordHash, "NewPass1"));
    EXPECT_FALSE(hasher.verify(updated->passwordHash, "OldPass1"));
}

TEST(ChangePasswordTest, WrongCurrentPassword_ThrowsAndLeavesPasswordUnchanged) {
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    ChangePassword changePassword(users, hasher);

    const User seeded = seedUser(users, hasher, "ivy@example.com", "OldPass1");

    EXPECT_THROW(changePassword.execute({seeded.id, "WrongPass1", "NewPass1"}),
                 InvalidCredentialsError);

    const auto stillOld = users.findById(seeded.id);
    ASSERT_TRUE(stillOld.has_value());
    EXPECT_TRUE(hasher.verify(stillOld->passwordHash, "OldPass1"));
}

// Same guard-clause ordering as RegisterUser: strength check runs before
// anything touches the repository or the hasher's verify step.
TEST(ChangePasswordTest, WeakNewPassword_ThrowsAndLeavesPasswordUnchanged) {
    InMemoryUserRepository users;
    Argon2PasswordHasher hasher;
    ChangePassword changePassword(users, hasher);

    const User seeded = seedUser(users, hasher, "jack@example.com", "OldPass1");

    EXPECT_THROW(changePassword.execute({seeded.id, "OldPass1", "weak"}),
                 kinetiq::domain::WeakPasswordError);

    const auto stillOld = users.findById(seeded.id);
    ASSERT_TRUE(stillOld.has_value());
    EXPECT_TRUE(hasher.verify(stillOld->passwordHash, "OldPass1"));
}
