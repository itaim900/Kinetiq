#pragma once

#include "domain/User.h"
#include "ports/IPasswordHasher.h"
#include "ports/ITokenService.h"
#include "ports/IUserRepository.h"

#include <stdexcept>
#include <string>

namespace kinetiq::app {

struct LoginUserRequest {
    std::string email;
    std::string password;
};

struct LoginUserResult {
    kinetiq::domain::User user;
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
    LoginUser(kinetiq::ports::IUserRepository& users, kinetiq::ports::IPasswordHasher& hasher,
              kinetiq::ports::ITokenService& tokens)
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
    kinetiq::ports::IUserRepository& users_;
    kinetiq::ports::IPasswordHasher& hasher_;
    kinetiq::ports::ITokenService& tokens_;
};

}  // namespace kinetiq::app
