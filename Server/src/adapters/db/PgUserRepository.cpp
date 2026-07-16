#include "adapters/db/PgUserRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::User rowToUser(const pqxx::row& row) {
    kinetiq::domain::User user;
    user.id = row["id"].as<long long>();
    user.email = row["email"].as<std::string>();
    user.passwordHash = row["password_hash"].as<std::string>();
    user.displayName = row["display_name"].is_null() ? "" : row["display_name"].as<std::string>();
    return user;
}

}  // namespace

kinetiq::domain::User PgUserRepository::save(const kinetiq::domain::User& user) {
    try {
        const auto result = tx_.exec(
            "INSERT INTO users (email, password_hash, display_name) "
            "VALUES ($1, $2, $3) RETURNING id",
            pqxx::params{user.email, user.passwordHash, user.displayName});

        kinetiq::domain::User stored = user;
        stored.id = result[0][0].as<long long>();
        return stored;
    } catch (const pqxx::unique_violation&) {
        // Users.email is `citext` with a UNIQUE constraint -- Postgres itself
        // caught the duplicate. Translate its exception into the port's own,
        // so callers never need to know they're talking to Postgres.
        throw kinetiq::ports::DuplicateEmailError(user.email);
    }
}

std::optional<kinetiq::domain::User> PgUserRepository::findByEmail(const std::string& email) {
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name FROM users WHERE email = $1",
        pqxx::params{email});
    if (result.empty()) return std::nullopt;
    return rowToUser(result[0]);
}

std::optional<kinetiq::domain::User> PgUserRepository::findById(long long id) {
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name FROM users WHERE id = $1",
        pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToUser(result[0]);
}

void PgUserRepository::update(const kinetiq::domain::User& user) {
    try {
        tx_.exec(
            "UPDATE users SET email = $1, password_hash = $2, display_name = $3 WHERE id = $4",
            pqxx::params{user.email, user.passwordHash, user.displayName, user.id});
    } catch (const pqxx::unique_violation&) {
        throw kinetiq::ports::DuplicateEmailError(user.email);
    }
}

}  // namespace kinetiq::db
