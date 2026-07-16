#pragma once

#include "domain/User.h"
#include "ports/IPasswordHasher.h"
#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include <stdexcept>
#include <string>

namespace fitapp::app {

struct LoginUserRequest {
    std::string email;
    std::string password;
};

struct LoginUserResult {
    fitapp::domain::User user;
    std::string accessToken;
};

// Thrown for BOTH "email not found" and "wrong password" -- deliberately a
// single type, so nothing downstream can accidentally reveal which one
// occurred. Never reveal whether an email is registered (locked decision,
// docs/02-api-contract.md: login must return an identical 401 either way).
class InvalidCredentialsError : public std::runtime_error {
public:
    InvalidCredentialsError() : std::runtime_error("invalid email or password") {}
};

class LoginUser {
public:
    LoginUser(fitapp::ports::IUserRepository& users, fitapp::ports::IPasswordHasher& hasher,
              fitapp::ports::ITokenService& tokens)
        : users_(users), hasher_(hasher), tokens_(tokens) {}

    LoginUserResult execute(const LoginUserRequest& request) {
        const auto found = users_.findByEmail(request.email);
        if (!found.has_value() || !hasher_.verify(found->passwordHash, request.password)) {
            throw InvalidCredentialsError();
        }

        const std::string token = tokens_.issue(found->id);
        return LoginUserResult{*found, token};
    }

private:
    fitapp::ports::IUserRepository& users_;
    fitapp::ports::IPasswordHasher& hasher_;
    fitapp::ports::ITokenService& tokens_;
};

}  // namespace fitapp::app
