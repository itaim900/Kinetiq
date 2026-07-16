#pragma once

#include "domain/User.h"

#include <optional>
#include <stdexcept>
#include <string>

namespace fitapp::ports {

// Thrown by save() when the email is already registered (case-insensitive --
// mirrors the `citext` UNIQUE constraint on Users.email in Postgres).
class DuplicateEmailError : public std::runtime_error {
public:
    explicit DuplicateEmailError(const std::string& email)
        : std::runtime_error("email already registered: " + email) {}
};

// Repository port for User persistence.
// Implementations: InMemoryUserRepository (tests/support/, fake) and
// PgUserRepository (adapters/db/, real) -- both must satisfy the exact same
// contract, so tests written against the fake hold for the real one too.
class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    // Assigns an id and persists the user. Throws DuplicateEmailError if the
    // email (case-insensitive) is already registered.
    virtual domain::User save(const domain::User& user) = 0;

    // Case-insensitive lookup (mirrors citext). Returns std::nullopt if no
    // user has that email.
    virtual std::optional<domain::User> findByEmail(const std::string& email) = 0;

    // Looks up a user by id (e.g. from an authenticated JWT's subject
    // claim). Returns std::nullopt if no user has that id.
    virtual std::optional<domain::User> findById(long long id) = 0;

    // Overwrites the stored fields of an existing user (matched by
    // user.id) -- e.g. a new password_hash after ChangePassword. Unlike
    // save(), this never assigns an id or checks for duplicates; the caller
    // is expected to have gotten `user` from find*() first.
    virtual void update(const domain::User& user) = 0;
};

}  // namespace fitapp::ports
