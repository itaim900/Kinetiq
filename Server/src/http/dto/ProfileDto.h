#pragma once

#include "app/UpdateProfile.h"
#include "domain/User.h"

#include "crow_all.h"

#include <optional>
#include <string>

namespace kinetiq::http::dto {

// Parses a PATCH /users/me body into a ProfileUpdate, JSON Merge Patch style:
// an absent key leaves the field unchanged, a value sets it, and null clears
// it (goes into `clear`; ADR-0017).
inline kinetiq::app::ProfileUpdate parseProfileUpdate(const crow::json::rvalue& body) {
    kinetiq::app::ProfileUpdate u;
    const auto isNull = [&body](const char* key) {
        return body.has(key) && body[key].t() == crow::json::type::Null;
    };
    for (const char* key : {"sex", "birth_date", "height_cm", "weight_kg", "experience_level",
                            "unit_preference", "weekly_workout_goal", "available_time_minutes",
                            "medications"}) {
        if (isNull(key)) u.clear.insert(key);
    }
    const auto present = [&](const char* key) { return body.has(key) && !isNull(key); };
    if (present("sex")) u.sex = std::string(body["sex"].s());
    if (present("birth_date")) u.birthDate = std::string(body["birth_date"].s());
    if (present("height_cm")) u.heightCm = body["height_cm"].d();
    if (present("weight_kg")) u.weightKg = body["weight_kg"].d();
    if (present("experience_level")) u.experienceLevel = std::string(body["experience_level"].s());
    if (present("unit_preference")) u.unitPreference = std::string(body["unit_preference"].s());
    if (present("weekly_workout_goal"))
        u.weeklyWorkoutGoal = static_cast<int>(body["weekly_workout_goal"].i());
    if (present("available_time_minutes"))
        u.availableTimeMinutes = static_cast<int>(body["available_time_minutes"].i());
    if (present("medications")) u.medications = std::string(body["medications"].s());
    return u;
}

namespace detail {
// crow::json::wvalue for an optional: the value, or JSON null when unset.
template <typename T>
inline crow::json::wvalue orNull(const std::optional<T>& opt) {
    return opt ? crow::json::wvalue(*opt) : crow::json::wvalue(nullptr);
}
}  // namespace detail

// Full profile serialization. Unset fields serialize as JSON null so the
// client can bind every form field. Never emits password_hash. Excludes
// `preferences` (out of scope this slice, D4).
inline crow::json::wvalue profileToJson(const kinetiq::domain::User& user) {
    crow::json::wvalue json;
    json["id"] = user.id;
    json["email"] = user.email;
    json["display_name"] = user.displayName;
    json["sex"] = detail::orNull(user.sex);
    json["birth_date"] = detail::orNull(user.birthDate);
    json["height_cm"] = detail::orNull(user.heightCm);
    json["weight_kg"] = detail::orNull(user.weightKg);
    json["experience_level"] = detail::orNull(user.experienceLevel);
    json["unit_preference"] = user.unitPreference;
    json["weekly_workout_goal"] = detail::orNull(user.weeklyWorkoutGoal);
    json["available_time_minutes"] = detail::orNull(user.availableTimeMinutes);
    json["medications"] = detail::orNull(user.medications);
    return json;
}

// RFC 9457 validation error with a per-field `errors` map (contract shape).
inline crow::response validationErrorResponse(const std::string& field,
                                              const std::string& message) {
    crow::json::wvalue json;
    json["type"] = "validation_error";
    json["title"] = "Validation Failed";
    json["status"] = 422;
    json["detail"] = "One or more fields are invalid.";
    json["errors"][field][0] = message;
    return crow::response(422, json);
}

}  // namespace kinetiq::http::dto
