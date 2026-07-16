#pragma once

#include "app/LoginUser.h"
#include "domain/PasswordPolicy.h"
#include "domain/User.h"
#include "ports/IPasswordHasher.h"
#include "ports/IUserRepository.h"

#include <string>

namespace fitapp::app {

struct ChangePasswordRequest {
    long long userId;
    std::string currentPassword;
    std::string newPassword;
};

// Use case: change password while already authenticated (PATCH
// /auth/password). No email needed, unlike the deferred "forgot password"
// reset flow -- the caller already proves they know the current password.
class ChangePassword {
public:
    ChangePassword(fitapp::ports::IUserRepository& users, fitapp::ports::IPasswordHasher& hasher)
        : users_(users), hasher_(hasher) {}

    // Throws fitapp::domain::WeakPasswordError if the new password fails
    // the policy (checked first -- neither the repository nor the hasher's
    // verify step is touched before this), or InvalidCredentialsError if
    // the current password is wrong (reusing LoginUser's error type: same
    // "don't leak more than necessary" principle).
    void execute(const ChangePasswordRequest& request) {
        fitapp::domain::requireStrongPassword(request.newPassword);

        const auto found = users_.findById(request.userId);
        if (!found.has_value() || !hasher_.verify(found->passwordHash, request.currentPassword)) {
            throw InvalidCredentialsError();
        }

        fitapp::domain::User updated = *found;
        updated.passwordHash = hasher_.hash(request.newPassword);
        users_.update(updated);
    }

private:
    fitapp::ports::IUserRepository& users_;
    fitapp::ports::IPasswordHasher& hasher_;
};

}  // namespace fitapp::app
