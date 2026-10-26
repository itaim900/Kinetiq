#pragma once

#include "domain/User.h"

#include <cstdio>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace kinetiq::domain {

// Thrown when a profile field is invalid. Carries the offending field's
// wire name (e.g. "weight_kg") so the HTTP layer can build the RFC 9457
// `errors: { field: [...] }` object.
class ProfileValidationError : public std::runtime_error {
public:
    ProfileValidationError(std::string field, const std::string& message)
        : std::runtime_error(message), field_(std::move(field)) {}
    const std::string& field() const { return field_; }

private:
    std::string field_;
};

namespace detail {

inline bool isOneOf(const std::string& v, std::initializer_list<const char*> allowed) {
    for (const char* a : allowed) {
        if (v == a) return true;
    }
    return false;
}

// Plausible YYYY-MM-DD: correct shape and sane calendar ranges. Deliberately
// does NOT check "not in the future" against the wall clock -- that would make
// the validator (and its tests) time-dependent; the 2100 ceiling is enough for
// v1. A strict not-future check is a documented deferral.
inline bool isPlausibleBirthDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    int y = 0, m = 0, d = 0;
    if (std::sscanf(s.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) return false;
    return y >= 1900 && y <= 2100 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

}  // namespace detail

// Validates every provided profile field on `u`. Throws ProfileValidationError
// on the first invalid field; returns normally if all present fields are valid.
// Unset optionals are skipped (an absent field is always valid).
inline void requireValidProfile(const User& u) {
    if (u.weightKg && (*u.weightKg <= 0.0 || *u.weightKg > 500.0)) {
        throw ProfileValidationError("weight_kg", "weight_kg must be between 0 and 500");
    }
    if (u.heightCm && (*u.heightCm <= 0.0 || *u.heightCm > 300.0)) {
        throw ProfileValidationError("height_cm", "height_cm must be between 0 and 300");
    }
    if (u.experienceLevel &&
        !detail::isOneOf(*u.experienceLevel, {"beginner", "intermediate", "advanced"})) {
        throw ProfileValidationError("experience_level",
                                     "experience_level must be beginner, intermediate, or advanced");
    }
    if (u.sex && !detail::isOneOf(*u.sex, {"male", "female", "other", "unspecified"})) {
        throw ProfileValidationError("sex", "sex must be male, female, other, or unspecified");
    }
    if (!detail::isOneOf(u.unitPreference, {"metric", "imperial"})) {
        throw ProfileValidationError("unit_preference", "unit_preference must be metric or imperial");
    }
    if (u.weeklyWorkoutGoal && *u.weeklyWorkoutGoal < 0) {
        throw ProfileValidationError("weekly_workout_goal", "weekly_workout_goal must be >= 0");
    }
    if (u.availableTimeMinutes && *u.availableTimeMinutes <= 0) {
        throw ProfileValidationError("available_time_minutes", "available_time_minutes must be > 0");
    }
    if (u.birthDate && !detail::isPlausibleBirthDate(*u.birthDate)) {
        throw ProfileValidationError("birth_date", "birth_date must be a valid YYYY-MM-DD date");
    }
}

}  // namespace kinetiq::domain
