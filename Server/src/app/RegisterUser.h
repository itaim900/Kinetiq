#pragma once

#include "domain/PasswordPolicy.h"
#include "domain/User.h"
#include "ports/IPasswordHasher.h"
#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include <string>

namespace fitapp::app {

struct RegisterUserRequest {
    std::string email;
    std::string password;
    std::string displayName;
};

struct RegisterUserResult {
    fitapp::domain::User user;
    std::string accessToken;
};

// Use case: hash the password, persist the user, issue an access token.
// Depends only on ports (never concrete adapters), per app/README.md, so it
// is unit-testable against fakes -- the caller decides which
// IUserRepository/IPasswordHasher/ITokenService implementation to inject.
class RegisterUser {
public:
    RegisterUser(fitapp::ports::IUserRepository& users, fitapp::ports::IPasswordHasher& hasher,
                 fitapp::ports::ITokenService& tokens)
        : users_(users), hasher_(hasher), tokens_(tokens) {}

    // Throws fitapp::domain::WeakPasswordError if the password fails the
    // strength policy, or fitapp::ports::DuplicateEmailError if the email is
    // already registered (propagated straight from the repository). Either
    // way, nothing is hashed or persisted before the throw.
    RegisterUserResult execute(const RegisterUserRequest& request) {
        fitapp::domain::requireStrongPassword(request.password);

        fitapp::domain::User user;
        user.email = request.email;
        user.displayName = request.displayName;
        user.passwordHash = hasher_.hash(request.password);

        const fitapp::domain::User saved = users_.save(user);
        const std::string token = tokens_.issue(saved.id);

        return RegisterUserResult{saved, token};
    }

private:
    fitapp::ports::IUserRepository& users_;
    fitapp::ports::IPasswordHasher& hasher_;
    fitapp::ports::ITokenService& tokens_;
};

}  // namespace fitapp::app
