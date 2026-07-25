#pragma once

#include "app/InjuryService.h"
#include "domain/Injury.h"

#include "crow_all.h"

#include <string>
#include <vector>

namespace kinetiq::http::dto {

// Parses POST body into an AddInjuryRequest, pairing it with the authenticated
// user id (never from the body).
inline kinetiq::app::AddInjuryRequest parseAddInjury(const crow::json::rvalue& body,
                                                     long long userId) {
    kinetiq::app::AddInjuryRequest req;
    req.userId = userId;
    req.bodyRegion = std::string(body["body_region"].s());
    req.severity = std::string(body["severity"].s());
    if (body.has("notes")) req.notes = std::string(body["notes"].s());
    return req;
}

// Parses PATCH body into UpdateInjuryFields -- only present keys are set.
inline kinetiq::app::UpdateInjuryFields parseUpdateInjury(const crow::json::rvalue& body) {
    kinetiq::app::UpdateInjuryFields fields;
    if (body.has("body_region")) fields.bodyRegion = std::string(body["body_region"].s());
    if (body.has("severity")) fields.severity = std::string(body["severity"].s());
    if (body.has("notes")) fields.notes = std::string(body["notes"].s());
    if (body.has("active")) fields.active = body["active"].b();
    return fields;
}

inline crow::json::wvalue injuryToJson(const kinetiq::domain::Injury& injury) {
    crow::json::wvalue json;
    json["id"] = injury.id;
    json["user_id"] = injury.userId;
    json["body_region"] = injury.bodyRegion;
    json["severity"] = injury.severity;
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
