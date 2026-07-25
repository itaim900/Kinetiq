#include "adapters/db/PgUserRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::User rowToUser(const pqxx::row& row) {
    kinetiq::domain::User user;
    user.id = row["id"].as<long long>();
    user.email = row["email"].as<std::string>();
    user.passwordHash = row["password_hash"].as<std::string>();
    user.displayName = row["display_name"].is_null() ? "" : row["display_name"].as<std::string>();

    if (!row["sex"].is_null()) user.sex = row["sex"].as<std::string>();
    if (!row["birth_date"].is_null()) user.birthDate = row["birth_date"].as<std::string>();
    if (!row["height_cm"].is_null()) user.heightCm = row["height_cm"].as<double>();
    if (!row["weight_kg"].is_null()) user.weightKg = row["weight_kg"].as<double>();
    if (!row["experience_level"].is_null())
        user.experienceLevel = row["experience_level"].as<std::string>();
    user.unitPreference =
        row["unit_preference"].is_null() ? "metric" : row["unit_preference"].as<std::string>();
    if (!row["weekly_workout_goal"].is_null())
        user.weeklyWorkoutGoal = row["weekly_workout_goal"].as<int>();
    if (!row["available_time_minutes"].is_null())
        user.availableTimeMinutes = row["available_time_minutes"].as<int>();
    if (!row["medications"].is_null()) user.medications = row["medications"].as<std::string>();
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
        "SELECT id, email, password_hash, display_name, sex, birth_date, height_cm, weight_kg, "
        "experience_level, unit_preference, weekly_workout_goal, available_time_minutes, "
        "medications FROM users WHERE email = $1",
        pqxx::params{email});
    if (result.empty()) return std::nullopt;
    return rowToUser(result[0]);
}

std::optional<kinetiq::domain::User> PgUserRepository::findById(long long id) {
    const auto result = tx_.exec(
        "SELECT id, email, password_hash, display_name, sex, birth_date, height_cm, weight_kg, "
        "experience_level, unit_preference, weekly_workout_goal, available_time_minutes, "
        "medications FROM users WHERE id = $1",
        pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToUser(result[0]);
}

void PgUserRepository::update(const kinetiq::domain::User& user) {
    try {
        tx_.exec(
            "UPDATE users SET email = $1, password_hash = $2, display_name = $3, sex = $4, "
            "birth_date = $5, height_cm = $6, weight_kg = $7, experience_level = $8, "
            "unit_preference = $9, weekly_workout_goal = $10, available_time_minutes = $11, "
            "medications = $12 WHERE id = $13",
            pqxx::params{user.email, user.passwordHash, user.displayName, user.sex, user.birthDate,
                         user.heightCm, user.weightKg, user.experienceLevel, user.unitPreference,
                         user.weeklyWorkoutGoal, user.availableTimeMinutes, user.medications,
                         user.id});
    } catch (const pqxx::unique_violation&) {
        throw kinetiq::ports::DuplicateEmailError(user.email);
    }
}

}  // namespace kinetiq::db
