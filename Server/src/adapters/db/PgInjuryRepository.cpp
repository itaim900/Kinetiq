#include "adapters/db/PgInjuryRepository.h"

namespace kinetiq::db {

namespace {

constexpr const char* kSelect =
    "SELECT ui.id, ui.user_id, ui.body_region, ui.severity, ui.injury_type, ui.active, "
    "ui.notes, ui.created_at, ui.condition_id, ic.name AS condition_name, "
    "ic.icd11_code AS condition_icd11 "
    "FROM user_injuries ui LEFT JOIN injury_conditions ic ON ic.id = ui.condition_id";

kinetiq::domain::Injury rowToInjury(const pqxx::row& row) {
    kinetiq::domain::Injury injury;
    injury.id = row["id"].as<long long>();
    injury.userId = row["user_id"].as<long long>();
    injury.bodyRegion = row["body_region"].as<std::string>();
    injury.severity = row["severity"].is_null() ? "" : row["severity"].as<std::string>();
    injury.injuryType = row["injury_type"].as<std::string>();
    injury.active = row["active"].as<bool>();
    if (!row["notes"].is_null()) injury.notes = row["notes"].as<std::string>();
    injury.createdAt = row["created_at"].as<std::string>();
    if (!row["condition_id"].is_null()) {
        kinetiq::domain::ConditionRef ref;
        ref.id = row["condition_id"].as<long long>();
        ref.name = row["condition_name"].as<std::string>();
        if (!row["condition_icd11"].is_null()) {
            ref.icd11Code = row["condition_icd11"].as<std::string>();
        }
        injury.condition = ref;
    }
    return injury;
}

std::optional<long long> conditionIdOf(const kinetiq::domain::Injury& injury) {
    if (injury.condition) return injury.condition->id;
    return std::nullopt;
}

}  // namespace

void PgInjuryRepository::loadMuscles(kinetiq::domain::Injury& injury) {
    injury.muscles.clear();
    // (role = 'secondary') is false for primary rows, and false sorts first.
    const auto result =
        injury.condition
            ? tx_.exec("SELECT m.id, m.name, cm.role FROM injury_condition_muscles cm "
                       "JOIN muscles m ON m.id = cm.muscle_id WHERE cm.condition_id = $1 "
                       "ORDER BY (cm.role = 'secondary'), m.name",
                       pqxx::params{injury.condition->id})
            : tx_.exec("SELECT m.id, m.name, 'primary' AS role FROM user_injury_muscles um "
                       "JOIN muscles m ON m.id = um.muscle_id WHERE um.injury_id = $1 "
                       "ORDER BY m.name",
                       pqxx::params{injury.id});
    for (const auto& row : result) {
        injury.muscles.push_back({row["id"].as<long long>(), row["name"].as<std::string>(),
                                  row["role"].as<std::string>()});
    }
}

void PgInjuryRepository::writeUserMuscles(const kinetiq::domain::Injury& injury) {
    tx_.exec("DELETE FROM user_injury_muscles WHERE injury_id = $1", pqxx::params{injury.id});
    if (injury.condition) return;  // known conditions read muscles from the catalog
    for (const auto& m : injury.muscles) {
        tx_.exec("INSERT INTO user_injury_muscles (injury_id, muscle_id) VALUES ($1, $2)",
                 pqxx::params{injury.id, m.muscleId});
    }
}

kinetiq::domain::Injury PgInjuryRepository::create(const kinetiq::domain::Injury& injury) {
    const auto result = tx_.exec(
        "INSERT INTO user_injuries (user_id, body_region, severity, injury_type, condition_id, "
        "active, notes) VALUES ($1, $2, $3, $4, $5, $6, $7) RETURNING id",
        pqxx::params{injury.userId, injury.bodyRegion, injury.severity, injury.injuryType,
                     conditionIdOf(injury), injury.active, injury.notes});
    kinetiq::domain::Injury stored = injury;
    stored.id = result[0][0].as<long long>();
    writeUserMuscles(stored);
    return *findById(stored.id);  // read back: created_at, names, roles
}

std::vector<kinetiq::domain::Injury> PgInjuryRepository::listByUser(long long userId) {
    const auto result = tx_.exec(std::string(kSelect) + " WHERE ui.user_id = $1 ORDER BY ui.id",
                                 pqxx::params{userId});
    std::vector<kinetiq::domain::Injury> out;
    out.reserve(result.size());
    for (const auto& row : result) {
        auto injury = rowToInjury(row);
        loadMuscles(injury);
        out.push_back(std::move(injury));
    }
    return out;
}

std::optional<kinetiq::domain::Injury> PgInjuryRepository::findById(long long id) {
    const auto result = tx_.exec(std::string(kSelect) + " WHERE ui.id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    auto injury = rowToInjury(result[0]);
    loadMuscles(injury);
    return injury;
}

void PgInjuryRepository::update(const kinetiq::domain::Injury& injury) {
    tx_.exec(
        "UPDATE user_injuries SET body_region = $1, severity = $2, injury_type = $3, "
        "active = $4, notes = $5 WHERE id = $6",
        pqxx::params{injury.bodyRegion, injury.severity, injury.injuryType, injury.active,
                     injury.notes, injury.id});
    writeUserMuscles(injury);
}

void PgInjuryRepository::deleteById(long long id) {
    tx_.exec("DELETE FROM user_injuries WHERE id = $1", pqxx::params{id});
}

}  // namespace kinetiq::db
