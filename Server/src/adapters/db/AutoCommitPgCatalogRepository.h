#pragma once

#include "ports/ICatalogRepository.h"

#include <string>

namespace kinetiq::db {

// Production ICatalogRepository: a fresh connection + transaction per call,
// delegating to PgCatalogRepository. Same rationale as the other AutoCommit
// repositories (routes capture deps for the server's lifetime; a pqxx
// transaction must not be shared across concurrent requests).
class AutoCommitPgCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    explicit AutoCommitPgCatalogRepository(std::string connString)
        : connString_(std::move(connString)) {}

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override;
    bool bodyRegionExists(const std::string& slug) override;
    std::vector<kinetiq::domain::Muscle> listMuscles() override;
    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override;
    std::vector<kinetiq::domain::InjuryCondition> listConditions() override;
    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
