#include "adapters/db/PgUserRepository.h"

namespace fitapp::db {

fitapp::domain::User PgUserRepository::save(const fitapp::domain::User& user) {
    try {
        const auto result = tx_.exec(
            "INSERT INTO users (email, password_hash, display_name) "
            "VALUES ($1, $2, $3) RETURNING id",
            pqxx::params{user.email, user.passwordHash, user.displayName});

        fitapp::domain::User stored = user;
        stored.id = result[0][0].as<long long>();
        return stored;
    } catch (const pqxx::unique_violation&) {
        // Users.email is `citext` with a UNIQUE constraint -- Postgres itself
        // caught the duplicate. Translate its exception into the port's own,
        // so callers never need to know they're talking to Postgres.
        throw fitapp::ports::DuplicateEmailError(user.email);
    }
}

std::optional<fitapp::domain::User> PgUserRepository::findByEmail(const std::string& email) {
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name FROM users WHERE email = $1",
        pqxx::params{email});
    if (result.empty()) return std::nullopt;

    const auto& row = result[0];
    fitapp::domain::User user;
    user.id = row["id"].as<long long>();
    user.email = row["email"].as<std::string>();
    user.passwordHash = row["password_hash"].as<std::string>();
    user.displayName = row["display_name"].is_null() ? "" : row["display_name"].as<std::string>();
    return user;
}

}  // namespace fitapp::db
