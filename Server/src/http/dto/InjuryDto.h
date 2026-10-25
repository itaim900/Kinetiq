#pragma once

#include "app/InjuryService.h"
#include "domain/Injury.h"
#include "http/dto/CatalogDto.h"

#include "crow_all.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace kinetiq::http::dto {

// Reads a JSON array of integers (e.g. "muscle_ids": [2, 3]). A non-array
// throws, which the controller maps to a generic 422.
inline std::vector<long long> parseIdList(const crow::json::rvalue& value) {
    if (value.t() != crow::json::type::List) {
        throw std::runtime_error("expected a list of ids");
    }
    std::vector<long long> ids;
    for (const auto& item : value) ids.push_back(item.i());
    return ids;
}

// Parses POST body into an AddInjuryRequest, pairing it with the authenticated
// user id (never from the body). Only present keys are set; InjuryService
// decides whether the combination is a valid known/unknown shape.
inline kinetiq::app::AddInjuryRequest parseAddInjury(const crow::json::rvalue& body,
                                                     long long userId) {
    kinetiq::app::AddInjuryRequest req;
    req.userId = userId;
    if (body.has("condition_id")) req.conditionId = body["condition_id"].i();
    if (body.has("body_region")) req.bodyRegion = std::string(body["body_region"].s());
    if (body.has("injury_type")) req.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_ids")) req.muscleIds = parseIdList(body["muscle_ids"]);
    req.severity = std::string(body["severity"].s());
    if (body.has("notes")) req.notes = std::string(body["notes"].s());
    return req;
}

// Parses PATCH body into UpdateInjuryFields -- only present keys are set.
inline kinetiq::app::UpdateInjuryFields parseUpdateInjury(const crow::json::rvalue& body) {
    kinetiq::app::UpdateInjuryFields fields;
    if (body.has("body_region")) fields.bodyRegion = std::string(body["body_region"].s());
    if (body.has("severity")) fields.severity = std::string(body["severity"].s());
    if (body.has("injury_type")) fields.injuryType = std::string(body["injury_type"].s());
    if (body.has("muscle_ids")) fields.muscleIds = parseIdList(body["muscle_ids"]);
    if (body.has("notes")) {
        // JSON null (or "") clears the notes.
        fields.notes = body["notes"].t() == crow::json::type::Null ? std::string()
                                                                    : std::string(body["notes"].s());
    }
    if (body.has("active")) fields.active = body["active"].b();
    return fields;
}

inline crow::json::wvalue injuryToJson(const kinetiq::domain::Injury& injury) {
    crow::json::wvalue json;
    json["id"] = injury.id;
    json["user_id"] = injury.userId;
    if (injury.condition) {
        crow::json::wvalue c;
        c["id"] = injury.condition->id;
        c["name"] = injury.condition->name;
        c["icd11_code"] = injury.condition->icd11Code
                              ? crow::json::wvalue(*injury.condition->icd11Code)
                              : crow::json::wvalue(nullptr);
        json["condition"] = std::move(c);
    } else {
        json["condition"] = crow::json::wvalue(nullptr);
    }
    json["body_region"] = injury.bodyRegion;
    json["injury_type"] = injury.injuryType;
    json["severity"] = injury.severity;
    json["muscles"] = injuryMuscleListToJson(injury.muscles);
    json["active"] = injury.active;
    json["notes"] = injury.notes ? crow::json::wvalue(*injury.notes) : crow::json::wvalue(nullptr);
    json["created_at"] = injury.createdAt;
    return json;
}

// Builds a JSON array. Uses the wvalue(list) constructor (list =
// std::vector<wvalue>) so an empty list serializes as [] (not null).
inline crow::json::wvalue injuryListToJson(const std::vector<kinetiq::domain::Injury>& injuries) {
    std::vector<crow::json::wvalue> items;
    items.reserve(injuries.size());
    for (const auto& injury : injuries) items.push_back(injuryToJson(injury));
    return crow::json::wvalue(items);
}

}  // namespace kinetiq::http::dto
