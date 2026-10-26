#pragma once

#include "ports/ICatalogRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

// Postgres-backed ICatalogRepository over the reference tables from migration
// 0004. Operates within a transaction it is handed (DbTest rollback in tests,
// a per-op transaction in prod via AutoCommitPgCatalogRepository).
class PgCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    explicit PgCatalogRepository(pqxx::work& tx) : tx_(tx) {}

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override;
    bool bodyRegionExists(const std::string& slug) override;
    std::vector<kinetiq::domain::Muscle> listMuscles() override;
    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override;
    std::vector<kinetiq::domain::InjuryCondition> listConditions() override;
    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override;

private:
    // A condition's muscles, primary first, then by name.
    std::vector<kinetiq::domain::InjuryMuscle> conditionMuscles(long long conditionId);

    pqxx::work& tx_;
};

}  // namespace kinetiq::db
