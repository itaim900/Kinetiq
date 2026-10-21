#pragma once

#include "domain/BodyRegion.h"
#include "domain/InjuryCondition.h"
#include "domain/Muscle.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::ports {

// Read-only reference data the app ships with: body regions, muscles and known
// injury conditions. Implementations: InMemoryCatalogRepository (tests/support/,
// fake) and PgCatalogRepository (adapters/db/, real).
class ICatalogRepository {
public:
    virtual ~ICatalogRepository() = default;

    virtual std::vector<domain::BodyRegion> listBodyRegions() = 0;
    virtual bool bodyRegionExists(const std::string& slug) = 0;
    virtual std::vector<domain::Muscle> listMuscles() = 0;
    virtual std::optional<domain::Muscle> findMuscle(long long id) = 0;
    virtual std::vector<domain::InjuryCondition> listConditions() = 0;
    virtual std::optional<domain::InjuryCondition> findCondition(long long id) = 0;
};

}  // namespace kinetiq::ports
