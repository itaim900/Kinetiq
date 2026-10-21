#pragma once

#include "ports/ICatalogRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::testsupport {

// Hand-written fake for ICatalogRepository, pre-seeded with a tiny catalog.
// Ids are fixed constants so tests can refer to them.
class InMemoryCatalogRepository : public kinetiq::ports::ICatalogRepository {
public:
    static constexpr long long kHamstrings = 1;
    static constexpr long long kQuads = 2;
    static constexpr long long kCalves = 3;
    static constexpr long long kGlutes = 4;
    static constexpr long long kHamstringStrain = 1;

    InMemoryCatalogRepository() {
        regions_ = {{"knee", "Knee"}, {"lower_back", "Lower back"}, {"thigh", "Thigh"}};
        muscles_ = {{kHamstrings, "Hamstrings", "legs"},
                    {kQuads, "Quads", "legs"},
                    {kCalves, "Calves", "legs"},
                    {kGlutes, "Glutes", "legs"}};
        kinetiq::domain::InjuryCondition c;
        c.id = kHamstringStrain;
        c.slug = "hamstring_strain";
        c.name = "Hamstring strain";
        c.icd11Code = "NC76.30";
        c.bodyRegion = "thigh";
        c.injuryType = "muscle";
        c.muscles = {{kHamstrings, "Hamstrings", "primary"}, {kGlutes, "Glutes", "secondary"}};
        conditions_.push_back(c);
    }

    std::vector<kinetiq::domain::BodyRegion> listBodyRegions() override { return regions_; }

    bool bodyRegionExists(const std::string& slug) override {
        for (const auto& r : regions_) {
            if (r.slug == slug) return true;
        }
        return false;
    }

    std::vector<kinetiq::domain::Muscle> listMuscles() override { return muscles_; }

    std::optional<kinetiq::domain::Muscle> findMuscle(long long id) override {
        for (const auto& m : muscles_) {
            if (m.id == id) return m;
        }
        return std::nullopt;
    }

    std::vector<kinetiq::domain::InjuryCondition> listConditions() override { return conditions_; }

    std::optional<kinetiq::domain::InjuryCondition> findCondition(long long id) override {
        for (const auto& c : conditions_) {
            if (c.id == id) return c;
        }
        return std::nullopt;
    }

private:
    std::vector<kinetiq::domain::BodyRegion> regions_;
    std::vector<kinetiq::domain::Muscle> muscles_;
    std::vector<kinetiq::domain::InjuryCondition> conditions_;
};

}  // namespace kinetiq::testsupport
