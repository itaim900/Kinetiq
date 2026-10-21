#include "adapters/db/PgInjuryRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::Injury rowToInjury(const pqxx::row& row) {
    kinetiq::domain::Injury injury;
    injury.id = row["id"].as<long long>();
    injury.userId = row["user_id"].as<long long>();
    injury.bodyRegion = row["body_region"].as<std::string>();
    injury.severity = row["severity"].is_null() ? "" : row["severity"].as<std::string>();
    injury.injuryType = row["injury_type"].is_null() ? "" : row["injury_type"].as<std::string>();
    if (!row["muscle_id"].is_null()) injury.muscleId = row["muscle_id"].as<long long>();
    injury.active = row["active"].as<bool>();
    if (!row["notes"].is_null()) injury.notes = row["notes"].as<std::string>();
    injury.createdAt = row["created_at"].as<std::string>();
    return injury;
}

constexpr const char* kColumns =
    "id, user_id, body_region, severity, injury_type, muscle_id, active, notes, created_at";

}  // namespace

kinetiq::domain::Injury PgInjuryRepository::create(const kinetiq::domain::Injury& injury) {
    const auto result = tx_.exec(
        "INSERT INTO user_injuries (user_id, body_region, severity, injury_type, muscle_id, "
        "active, notes) VALUES ($1, $2, $3, $4, $5, $6, $7) "
        "RETURNING " + std::string(kColumns),
        pqxx::params{injury.userId, injury.bodyRegion, injury.severity, injury.injuryType,
                     injury.muscleId, injury.active, injury.notes});
    return rowToInjury(result[0]);
}

std::vector<kinetiq::domain::Injury> PgInjuryRepository::listByUser(long long userId) {
    const auto result =
        tx_.exec("SELECT " + std::string(kColumns) +
                     " FROM user_injuries WHERE user_id = $1 ORDER BY id",
                 pqxx::params{userId});
    std::vector<kinetiq::domain::Injury> out;
    out.reserve(result.size());
    for (const auto& row : result) out.push_back(rowToInjury(row));
    return out;
}

std::optional<kinetiq::domain::Injury> PgInjuryRepository::findById(long long id) {
    const auto result = tx_.exec(
        "SELECT " + std::string(kColumns) + " FROM user_injuries WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToInjury(result[0]);
}

void PgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    tx_.exec(
        "UPDATE user_injuries SET body_region = $1, severity = $2, injury_type = $3, "
        "muscle_id = $4, active = $5, notes = $6 WHERE id = $7",
        pqxx::params{injury.bodyRegion, injury.severity, injury.injuryType, injury.muscleId,
                     injury.active, injury.notes, injury.id});
}

void PgInjuryRepository::deleteById(long long id) {
    tx_.exec("DELETE FROM user_injuries WHERE id = $1", pqxx::params{id});
}

}  // namespace kinetiq::db
