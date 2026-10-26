#pragma once

#include "domain/BodyRegion.h"
#include "domain/InjuryCondition.h"
#include "domain/InjuryMuscle.h"
#include "domain/Muscle.h"

#include "crow_all.h"

#include <vector>

namespace kinetiq::http::dto {

// [{id, name, role}] -- shared by injury and condition responses. Built with
// the wvalue(vector) constructor so an empty list serializes as [] (not null).
inline crow::json::wvalue injuryMuscleListToJson(
    const std::vector<kinetiq::domain::InjuryMuscle>& muscles) {
    std::vector<crow::json::wvalue> items;
    for (const auto& m : muscles) {
        crow::json::wvalue j;
        j["id"] = m.muscleId;
        j["name"] = m.name;
        j["role"] = m.role;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue bodyRegionListToJson(
    const std::vector<kinetiq::domain::BodyRegion>& regions) {
    std::vector<crow::json::wvalue> items;
    for (const auto& r : regions) {
        crow::json::wvalue j;
        j["slug"] = r.slug;
        j["name"] = r.name;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue muscleListToJson(const std::vector<kinetiq::domain::Muscle>& muscles) {
    std::vector<crow::json::wvalue> items;
    for (const auto& m : muscles) {
        crow::json::wvalue j;
        j["id"] = m.id;
        j["name"] = m.name;
        j["muscle_group"] = m.muscleGroup;
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

inline crow::json::wvalue conditionListToJson(
    const std::vector<kinetiq::domain::InjuryCondition>& conditions) {
    std::vector<crow::json::wvalue> items;
    for (const auto& c : conditions) {
        crow::json::wvalue j;
        j["id"] = c.id;
        j["slug"] = c.slug;
        j["name"] = c.name;
        j["icd11_code"] =
            c.icd11Code ? crow::json::wvalue(*c.icd11Code) : crow::json::wvalue(nullptr);
        j["body_region"] = c.bodyRegion;
        j["injury_type"] = c.injuryType;
        j["muscles"] = injuryMuscleListToJson(c.muscles);
        items.push_back(std::move(j));
    }
    return crow::json::wvalue(items);
}

}  // namespace kinetiq::http::dto
