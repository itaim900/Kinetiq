#include "adapters/db/PgCatalogRepository.h"

namespace kinetiq::db {

namespace {

kinetiq::domain::Muscle rowToMuscle(const pqxx::row& row) {
    return {row["id"].as<long long>(), row["name"].as<std::string>(),
            row["muscle_group"].as<std::string>()};
}

kinetiq::domain::InjuryCondition rowToCondition(const pqxx::row& row) {
    kinetiq::domain::InjuryCondition c;
    c.id = row["id"].as<long long>();
    c.slug = row["slug"].as<std::string>();
    c.name = row["name"].as<std::string>();
    if (!row["icd11_code"].is_null()) c.icd11Code = row["icd11_code"].as<std::string>();
    c.bodyRegion = row["body_region"].as<std::string>();
    c.injuryType = row["injury_type"].as<std::string>();
    return c;
}

constexpr const char* kSelectConditions =
    "SELECT id, slug, name, icd11_code, body_region, injury_type FROM injury_conditions";

}  // namespace

std::vector<kinetiq::domain::BodyRegion> PgCatalogRepository::listBodyRegions() {
    std::vector<kinetiq::domain::BodyRegion> out;
    for (const auto& row : tx_.exec("SELECT slug, name FROM body_regions ORDER BY name")) {
        out.push_back({row["slug"].as<std::string>(), row["name"].as<std::string>()});
    }
    return out;
}

bool PgCatalogRepository::bodyRegionExists(const std::string& slug) {
    return !tx_.exec("SELECT 1 FROM body_regions WHERE slug = $1", pqxx::params{slug}).empty();
}

std::vector<kinetiq::domain::Muscle> PgCatalogRepository::listMuscles() {
    std::vector<kinetiq::domain::Muscle> out;
    for (const auto& row :
         tx_.exec("SELECT id, name, muscle_group FROM muscles ORDER BY muscle_group, name")) {
        out.push_back(rowToMuscle(row));
    }
    return out;
}

std::optional<kinetiq::domain::Muscle> PgCatalogRepository::findMuscle(long long id) {
    const auto result =
        tx_.exec("SELECT id, name, muscle_group FROM muscles WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    return rowToMuscle(result[0]);
}

std::vector<kinetiq::domain::InjuryMuscle> PgCatalogRepository::conditionMuscles(
    long long conditionId) {
    std::vector<kinetiq::domain::InjuryMuscle> out;
    // (role = 'secondary') is false for primary rows, and false sorts first.
    for (const auto& row : tx_.exec(
             "SELECT m.id, m.name, cm.role FROM injury_condition_muscles cm "
             "JOIN muscles m ON m.id = cm.muscle_id WHERE cm.condition_id = $1 "
             "ORDER BY (cm.role = 'secondary'), m.name",
             pqxx::params{conditionId})) {
        out.push_back({row["id"].as<long long>(), row["name"].as<std::string>(),
                       row["role"].as<std::string>()});
    }
    return out;
}

std::vector<kinetiq::domain::InjuryCondition> PgCatalogRepository::listConditions() {
    std::vector<kinetiq::domain::InjuryCondition> out;
    // One muscles query per condition: 17 small queries in one transaction is
    // fine for a reference list this size, and keeps the code simple.
    for (const auto& row : tx_.exec(std::string(kSelectConditions) + " ORDER BY name")) {
        auto c = rowToCondition(row);
        c.muscles = conditionMuscles(c.id);
        out.push_back(std::move(c));
    }
    return out;
}

std::optional<kinetiq::domain::InjuryCondition> PgCatalogRepository::findCondition(long long id) {
    const auto result =
        tx_.exec(std::string(kSelectConditions) + " WHERE id = $1", pqxx::params{id});
    if (result.empty()) return std::nullopt;
    auto c = rowToCondition(result[0]);
    c.muscles = conditionMuscles(c.id);
    return c;
}

}  // namespace kinetiq::db
